package com.microsoft.speech.ttsclientsdk;

/**
 * Created by v-guoya on 5/5/2017.
 */

public class Synthesizer {

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("TTSClientSDK");
    }

    ITTSCallback callback;
    public void setReceiveWave(ITTSCallback callback) {
        this.callback = callback;
    }

    public int ReceiveWave(Object callBackStat, byte[] data, int size){
        return callback.ReceiveWave(callBackStat, data, size);
    }

    public long MSTTS_CreateSpeechSynthesizerHandler(long phSynthesizerHandle, String jstrApiKry){
        return CreateHandler(phSynthesizerHandle, jstrApiKry);
    }

    public int MSTTS_Speak(long phSynthesizerHandle, String jstrContent, int eContentType){
        return Speak(phSynthesizerHandle, jstrContent, eContentType);
    }

    public int MSTTS_SetOutput(long phSynthesizerHandle, Object callBackStat, Synthesizer object){
        return SetOutput(phSynthesizerHandle, callBackStat, object);
    }

    public void MSTTS_CloseSynthesizer(long phSynthesizerHandle){
        CloseHandler(phSynthesizerHandle);
    }

    public int MSTTS_Stop(long phSynthesizerHandle){
        return Stop(phSynthesizerHandle);
    }

    public TTSWaveFormat MSTTS_SetOutput(long phSynthesizerHandle){
        return GetOutput(phSynthesizerHandle);
    }

    public int MSTTS_SetVoice(long phSynthesizerHandle, String name, String lang){
        return SetVoice(phSynthesizerHandle, name, lang);
    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    private native long CreateHandler(long phSynthesizerHandle, String jstrApiKry);
    private native int SetOutput(long phSynthesizerHandle, Object callBackStat, Synthesizer usingObject);
    private native int Speak(long phSynthesizerHandle, String jstrContent, int eContentType);
    private native int SetVoice(long phSynthesizerHandle, String name, String lang);
    private native void CloseHandler(long phSynthesizerHandle);
    private native TTSWaveFormat GetOutput(long phSynthesizerHandle);
    private native int Stop(long phSynthesizerHandle);
}
