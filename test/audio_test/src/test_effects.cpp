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
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <memory.h>
#include <stdint.h>
#include <string>
#include <stdlib.h>

#include "avs_audio_effect.h"

static void progress_status_h(int progress, void *arg){
    printf("progress = %d pct \n", progress);
}

int main(int argc, char *argv[])
{
    std::string in_file_name;
    std::string out_file_name;
    int args;
    FILE *in_file, *out_file;
    int sample_rate_hz = -1;
    bool use_noise_reduction = false;
    
    audio_effect effect_type = AUDIO_EFFECT_CHORUS;
    
    // Handle Command Line inputs
    args = 0;
    while(args < argc){
        if (strcmp(argv[args], "-in")==0){
            args++;
            in_file_name.insert(0,argv[args]);
        } else if (strcmp(argv[args], "-out")==0){
            args++;
            out_file_name.insert(0,argv[args]);
        } else if (strcmp(argv[args], "-fs")==0){
            args++;
            sample_rate_hz = atol(argv[args]);
        } else if (strcmp(argv[args], "-nr")==0){
            use_noise_reduction = true;
        } else if (strcmp(argv[args], "-effect")==0){
            args++;
            if (strcmp(argv[args], "chorus_1")==0){
                effect_type = AUDIO_EFFECT_CHORUS_MIN;
            } else if (strcmp(argv[args], "chorus_2")==0){
                effect_type = AUDIO_EFFECT_CHORUS_MAX;
            } else if (strcmp(argv[args], "reverb_1")==0){
                effect_type = AUDIO_EFFECT_REVERB_MIN;
            } else if (strcmp(argv[args], "reverb_2")==0){
                effect_type = AUDIO_EFFECT_REVERB_MID;
            } else if (strcmp(argv[args], "reverb_3")==0){
                effect_type = AUDIO_EFFECT_REVERB_MAX;
            } else if (strcmp(argv[args], "pitch_up_1")==0){
                effect_type = AUDIO_EFFECT_PITCH_UP_SHIFT_MIN;
            } else if (strcmp(argv[args], "pitch_up_2")==0){
                effect_type = AUDIO_EFFECT_PITCH_UP_SHIFT_MED;
            } else if (strcmp(argv[args], "pitch_up_3")==0){
                effect_type = AUDIO_EFFECT_PITCH_UP_SHIFT_MAX;
            } else if (strcmp(argv[args], "pitch_up_4")==0){
                effect_type = AUDIO_EFFECT_PITCH_UP_SHIFT_INSANE;
            } else if (strcmp(argv[args], "pitch_down_1")==0){
                effect_type = AUDIO_EFFECT_PITCH_DOWN_SHIFT_MIN;
            } else if (strcmp(argv[args], "pitch_down_2")==0){
                effect_type = AUDIO_EFFECT_PITCH_DOWN_SHIFT_MED;
            } else if (strcmp(argv[args], "pitch_down_3")==0){
                effect_type = AUDIO_EFFECT_PITCH_DOWN_SHIFT_MAX;
            } else if (strcmp(argv[args], "pitch_down_4")==0){
                effect_type = AUDIO_EFFECT_PITCH_DOWN_SHIFT_INSANE;
            } else if (strcmp(argv[args], "pace_up_1")==0){
                effect_type = AUDIO_EFFECT_PACE_UP_SHIFT_MIN;
            } else if (strcmp(argv[args], "pace_up_2")==0){
                effect_type = AUDIO_EFFECT_PACE_UP_SHIFT_MED;
            } else if (strcmp(argv[args], "pace_up_3")==0){
                effect_type = AUDIO_EFFECT_PACE_UP_SHIFT_MAX;
            } else if (strcmp(argv[args], "pace_down_1")==0){
                effect_type = AUDIO_EFFECT_PACE_DOWN_SHIFT_MIN;
            } else if (strcmp(argv[args], "pace_down_2")==0){
                effect_type = AUDIO_EFFECT_PACE_DOWN_SHIFT_MED;
            } else if (strcmp(argv[args], "pace_down_3")==0){
                effect_type = AUDIO_EFFECT_PACE_DOWN_SHIFT_MAX;
            } else if (strcmp(argv[args], "reverse")==0){
                effect_type = AUDIO_EFFECT_REVERSE;
            } else if (strcmp(argv[args], "vocoder")==0){
                effect_type = AUDIO_EFFECT_VOCODER_MED;
            }

        }
        args++;
    }
    
    
    
    printf("\n------------------------------------------ \n");
    printf("Start Audio Effects test \n");
    printf("------------------------------------------ \n\n");
    
    if(sample_rate_hz > 0){
        apply_effect_to_pcm(in_file_name.c_str(), out_file_name.c_str(), sample_rate_hz, effect_type, use_noise_reduction, progress_status_h, NULL);
    } else {
        apply_effect_to_wav(in_file_name.c_str(), out_file_name.c_str(), effect_type, use_noise_reduction, progress_status_h, NULL);
    }
    return 0;
}
