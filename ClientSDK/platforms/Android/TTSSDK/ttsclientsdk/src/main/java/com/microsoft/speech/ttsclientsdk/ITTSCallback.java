package com.microsoft.speech.ttsclientsdk;

/**
 * Created by v-guoya on 5/5/2017.
 */

public interface ITTSCallback {
    public int ReceiveWave(Object callBackStat, byte[] data, int size);
}
