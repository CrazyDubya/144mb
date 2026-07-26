#ifndef AUDIO_WIN_H
#define AUDIO_WIN_H
#define AUD_HZ 44100
#define AUD_FRAMES 735
#define AUD_BUFFERS 4
static HWAVEOUT aw_wave;static WAVEHDR aw_hdr[AUD_BUFFERS];static int16_t aw_data[AUD_BUFFERS][AUD_FRAMES*2];static int aw_written[AUD_BUFFERS],aw_ok;
static void beta_audio_open(void){WAVEFORMATEX w={0};w.wFormatTag=WAVE_FORMAT_PCM;w.nChannels=2;w.nSamplesPerSec=AUD_HZ;w.wBitsPerSample=16;w.nBlockAlign=4;w.nAvgBytesPerSec=AUD_HZ*4;if(!waveOutGetNumDevs()||waveOutOpen(&aw_wave,WAVE_MAPPER,&w,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR)return;for(int i=0;i<AUD_BUFFERS;i++){aw_hdr[i].lpData=(LPSTR)aw_data[i];aw_hdr[i].dwBufferLength=sizeof aw_data[i];if(waveOutPrepareHeader(aw_wave,&aw_hdr[i],sizeof aw_hdr[i])!=MMSYSERR_NOERROR)return;}aw_ok=1;}
static void beta_audio_pump(void){if(!aw_ok)return;for(int i=0;i<AUD_BUFFERS;i++){if(aw_written[i]&&!(aw_hdr[i].dwFlags&WHDR_DONE))continue;game_audio(aw_data[i],AUD_FRAMES);if(waveOutWrite(aw_wave,&aw_hdr[i],sizeof aw_hdr[i])==MMSYSERR_NOERROR)aw_written[i]=1;return;}}
static void beta_audio_close(void){if(!aw_ok)return;waveOutReset(aw_wave);for(int i=0;i<AUD_BUFFERS;i++)waveOutUnprepareHeader(aw_wave,&aw_hdr[i],sizeof aw_hdr[i]);waveOutClose(aw_wave);}
#endif
