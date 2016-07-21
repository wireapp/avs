/*
* Wire
* Copyright (C) 2016 Wire Swiss GmbH
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include <pthread.h>
#include <stdio.h>
#include <re.h>

#include <avs.h>
#include <avs_vie.h>

#include "webrtc/common_types.h"
#include "webrtc/common.h"
#include "webrtc/video_encoder.h"
#include "vie_renderer.h"
#include "vie.h"

enum {
	MIN_SEND_BANDWIDTH = 100,  /* kilobits/second */
	MAX_SEND_BANDWIDTH = 800,  /* kilobits/second */
};

const size_t num_resolutions = 4;
const struct resolution_info {
	uint32_t width;
	uint32_t height;
	uint32_t max_fps;
	uint32_t min_br;
	uint32_t max_br;
}
resolutions[num_resolutions] = {
	{640, 480, 15, 500, 800},
	{480, 360, 15, 200, 600},
	{320, 240, 15, 100, 300},
	{240, 180, 15,   0, 150}
};


static enum flowmgr_video_send_state _send_state = FLOWMGR_VIDEO_SEND_NONE;

static int get_resolution_for_bitrate(uint32_t bitrate)
{
	size_t r = 0;

	while (r < num_resolutions - 1) {
		if (bitrate > (resolutions[r].min_br * 1000)) {
			break;
		}
		r++;
	}

	return r;
}

std::vector<webrtc::VideoStream> CreateVideoStream(size_t res_idx,
	bool rtp_rotation, int32_t max_bandwidth) {

	std::vector<webrtc::VideoStream> stream_settings(1);
	
	uint32_t width = resolutions[res_idx].width;
	uint32_t height = resolutions[res_idx].height;
	uint32_t fps = resolutions[res_idx].max_fps;

	stream_settings[0].width = rtp_rotation ? width : height;
	stream_settings[0].height = rtp_rotation ? height : width;
	stream_settings[0].max_framerate = fps;
	stream_settings[0].min_bitrate_bps = MIN_SEND_BANDWIDTH * 1000;

	stream_settings[0].target_bitrate_bps =
		stream_settings[0].max_bitrate_bps = max_bandwidth * 1000;
	stream_settings[0].max_qp = 56;

	return stream_settings;
}

webrtc::VideoEncoderConfig CreateEncoderConfig(size_t res_idx,
	bool rtp_rotation, int32_t max_bandwidth) {

	webrtc::VideoEncoderConfig encoder_config;
	encoder_config.streams = CreateVideoStream(res_idx, rtp_rotation, 
		max_bandwidth);
	return encoder_config;
}

void vie_frame_handler(webrtc::VideoFrame *frame, void *arg)
{
	webrtc::VideoCaptureInput *input = (webrtc::VideoCaptureInput *)arg;

	if (input != NULL && frame != NULL) {
		input->IncomingCapturedFrame(*frame);
	}
}

static void ves_destructor(void *arg)
{
	struct videnc_state *ves = (struct videnc_state *)arg;

	vie_capture_stop(ves);

	mem_deref(ves->sdpm);
	mem_deref(ves->vie);

	_send_state = FLOWMGR_VIDEO_SEND_NONE;
}

static bool check_rotation_attr(const char *name, const char *value, void *arg){
	if (0 == re_regex(value, strlen(value), "urn:3gpp:video-orientation")){
		return true;
		} else{
		return false;
	}
}

static bool sdp_has_rtp_rotation(struct videnc_state *ves){
	const char *rot;
	rot = sdp_media_rattr_apply(ves->sdpm, "extmap", check_rotation_attr, NULL);

	return rot ? true : false;
}

static int32_t sdp_get_max_bandwidth(struct videnc_state *ves){
	int32_t bw = sdp_media_rbandwidth(ves->sdpm, SDP_BANDWIDTH_AS);

	debug("%s: sdpbw: %d min: %d max: %d\n", __FUNCTION__, bw,
		MIN_SEND_BANDWIDTH, MAX_SEND_BANDWIDTH);

	if (bw < 0) {
		/* Remote hasnt specified, send my max */
		bw = MAX_SEND_BANDWIDTH;
	}
	else if (bw < MIN_SEND_BANDWIDTH) {
		/* Remotes max is less than my min, send my min */
		bw = MIN_SEND_BANDWIDTH;
	}
	else if (bw > MAX_SEND_BANDWIDTH) {
		/* Remote accepts more than my max, send my max */
		bw = MAX_SEND_BANDWIDTH;
	}
	/* else send bw */
	
	info("%s: setting max send bandwidth %d\n", __FUNCTION__, bw);
	return bw;
}

int vie_enc_alloc(struct videnc_state **vesp,
		  struct media_ctx **mctxp,
		  const struct vidcodec *vc,
		  const char *fmtp, int pt,
		  struct sdp_media *sdpm,
		  struct vidcodec_param *prm,
		  videnc_rtp_h *rtph,
		  videnc_rtcp_h *rtcph,
		  viddec_err_h *errh,
		  void *arg)
{
	struct videnc_state *ves;
	int err = 0;

	if (!vesp || !vc || !mctxp)
		return EINVAL;

	info("%s: allocating codec:%s(%d)\n", __FUNCTION__, vc->name, pt);

	ves = (struct videnc_state *)mem_zalloc(sizeof(*ves), ves_destructor);
	if (!ves)
		return ENOMEM;

	if (*mctxp) {
		ves->vie = (struct vie *)mem_ref(*mctxp);
	}
	else {
		err = vie_alloc(&ves->vie, vc, pt);
		if (err) {
			goto out;
		}

		*mctxp = (struct media_ctx *)ves->vie;
	}


	ves->vc = vc;
	ves->pt = pt;
	ves->sdpm = (struct sdp_media *)mem_ref(sdpm);
	ves->vie->ves = ves;
	ves->rtph = rtph;
	ves->rtcph = rtcph;
	ves->errh = errh;
	ves->arg = arg;
	if (prm)
		ves->prm = *prm;

 out:
	if (err) {
		mem_deref(ves);
	}
	else {
		*vesp = ves;
	}

	return err;
}

static bool rtx_format_handler(struct sdp_format *fmt, void *arg)
{
	struct videnc_state *ves = (struct videnc_state *)arg;
	int apt;

	sscanf(fmt->params, "apt=%d", &apt);

	return apt == ves->pt;
}


static int vie_capture_start_int(struct videnc_state *ves)
{
	struct vie *vie = ves ? ves->vie: NULL;
	int err = 0;

	if (!ves || !vie)
		return EINVAL;

#if USE_RTP_ROTATION
	ves->rtp_rotation = sdp_has_rtp_rotation(ves);
#else
	ves->rtp_rotation = false;
#endif

	ves->max_bandwidth = sdp_get_max_bandwidth(ves);
	ves->res_idx = get_resolution_for_bitrate(ves->max_bandwidth * 1000);

	info("%s: remote side %s support rotation\n", __FUNCTION__,
		ves->rtp_rotation ? "does" : "does not");
	webrtc::VideoSendStream::Config send_config(vie->transport);
	webrtc::VideoEncoderConfig encoder_config(CreateEncoderConfig(
		ves->res_idx, ves->rtp_rotation, ves->max_bandwidth));

	send_config.rtp.ssrcs.push_back(ves->prm.local_ssrcv[0]);
	send_config.rtp.nack.rtp_history_ms = 0;

	send_config.rtp.fec.red_payload_type = -1;	
	send_config.rtp.fec.red_rtx_payload_type = -1;	

#if USE_RTX
	if (ves->prm.local_ssrcc > 1) {
		sdp_format *rtx;
		
		rtx = sdp_media_format_apply(ves->sdpm, false, NULL, -1, "rtx",
					     -1, -1, rtx_format_handler, ves);

		if (!rtx) {
			warning("vie: %s: rtx_fmt not found\n", __func__);
		}
		else {
			debug("vie: %s: rtx ssrc=%u pt=%d\n",
			      __func__, ves->prm.local_ssrcv[1], rtx->pt);

			send_config.rtp.nack.rtp_history_ms = 5000;
			send_config.rtp.rtx.ssrcs.push_back(ves->prm.local_ssrcv[1]);

			send_config.rtp.rtx.payload_type = rtx->pt;
		}
	}
#endif

	vie->stats_rx.rtcp.ssrc = ves->prm.local_ssrcv[0];
	vie->stats_rx.rtcp.bitrate_limit = 0;

	send_config.rtp.extensions.push_back(
		webrtc::RtpExtension(webrtc::RtpExtension::kAbsSendTime,
		kAbsSendTimeExtensionId));
	if (ves->rtp_rotation) {
		send_config.rtp.extensions.push_back(
			webrtc::RtpExtension(webrtc::RtpExtension::kVideoRotation,
			kVideoRotationRtpExtensionId));
	}

	vie->encoder = webrtc::VideoEncoder::Create(webrtc::VideoEncoder::kVp8);

	send_config.encoder_settings.encoder = vie->encoder;
	send_config.encoder_settings.payload_name = ves->vc->name;
	send_config.encoder_settings.payload_type = ves->pt;
	send_config.suspend_below_min_bitrate = false;

	vie->send_stream = vie->call->CreateVideoSendStream(send_config,
							    encoder_config);
	if (vie->send_stream == NULL) {
		err = ENOENT;
		goto out;
	}

	vie_capture_router_attach_stream(vie->send_stream->Input(), 
		!ves->rtp_rotation);
	debug("capture_start_device\n");

	vie->send_stream->Start();

out:
	if (err != 0)
		error("%s: err=%d\n", __FUNCTION__, err);

	return err;
}

int vie_capture_start(struct videnc_state *ves)
{
	int err = 0;
	if (_send_state == FLOWMGR_VIDEO_SEND) {
		return 0;
	}

	debug("%s: ss %d\n", __FUNCTION__, _send_state);
	err = vie_capture_start_int(ves);
	if (err == 0) {
		_send_state = FLOWMGR_VIDEO_SEND;
	}

	return err;
}

static void vie_capture_stop_int(struct videnc_state *ves)
{
	struct vie *vie = ves ? ves->vie: NULL;

	if (!ves || _send_state == FLOWMGR_VIDEO_SEND_NONE) {
		return;
	}

	if (!vie || !vie->send_stream) {
		return;
	}

	vie->send_stream->Stop();

	vie_capture_router_detach_stream(vie->send_stream->Input());

	vie->call->DestroyVideoSendStream(vie->send_stream);
	vie->send_stream = NULL;
    if(vie->encoder){
        delete vie->encoder;
    }
	vie->encoder = NULL;
}

void vie_capture_stop(struct videnc_state *ves)
{
	debug("%s: ss %d\n", __FUNCTION__, _send_state);
	vie_capture_stop_int(ves);
	_send_state = FLOWMGR_VIDEO_SEND_NONE;
}

void vie_capture_hold(struct videnc_state *ves, bool hold)
{
#if 0
	struct vie *vie;

	if (!ves)
		return;	

	vie = ves->vie;
	
	if (hold) {	
		if (vie->send_stream) {
			vie->send_stream->Stop();
		}

		if (_capturer) {
			vie_capturer_detach_stream(_capturer, vie->send_stream->Input());
		}
	}
	else { /* Resume */
		/*
		webrtc::VideoCaptureCapability capture_caps;

		capture_caps.width = ves->capture_width;
		capture_caps.height = ves->capture_height;
		capture_caps.maxFPS = ves->capture_fps;
		capture_caps.rawType = webrtc::kVideoI420;
		*/
		if (_capturer) {
			if (vie_capturer_start(_capturer, ves->capture_width,
				ves->capture_height, ves->capture_fps) != 0) {
				warning("%s: failed to start capture\n",
					__func__);
			}
		}

		if (vie->send_stream) {
			vie->send_stream->Start();
		}
	}
#endif
}

void vie_bandwidth_allocation_changed(struct vie *vie, uint32_t ssrc, uint32_t allocation)
{
	struct videnc_state *ves = vie ? vie->ves : NULL;
	int r = 0;

	if (!vie || !ves || !vie->send_stream) {
		return;
	}

	if (ssrc != ves->prm.local_ssrcv[0]) {
		return;
	}

	if (allocation < resolutions[ves->res_idx].min_br * 1000 ||
		allocation > resolutions[ves->res_idx].max_br * 1000) {
		size_t target_res = get_resolution_for_bitrate(allocation);

		if (target_res != ves->res_idx) {
			info("%s send resolution changed from %ux%u to %ux%u br: %u\n",
				__FUNCTION__,
				resolutions[ves->res_idx].width,
				resolutions[ves->res_idx].height,
				resolutions[target_res].width,
				resolutions[target_res].height,
				allocation);

			webrtc::VideoEncoderConfig config = CreateEncoderConfig(target_res,
				ves->rtp_rotation, ves->max_bandwidth);
			vie->send_stream->ReconfigureVideoEncoder(config);
			ves->res_idx = target_res;
		}
	}
}


