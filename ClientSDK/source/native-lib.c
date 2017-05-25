#include <jni.h>
#include <TTSClientSDK.h>
#include <malloc.h>
#include <string.h>

typedef struct androidCallBackStat
{
    void* pCallBackStat;
    JNIEnv* env;
    jobject  usingObject;
}AndroidCallBackStat;

char* jstringTostring(JNIEnv* env, jstring jstr)
{
    char* rtn = NULL;
    jclass clsstring = (*env)->FindClass(env, "java/lang/String");
    jstring strencode = (*env)->NewStringUTF(env, "utf-8");
    jmethodID mid = (*env)->GetMethodID(env, clsstring, "getBytes", "(Ljava/lang/String;)[B");
    jbyteArray barr= (jbyteArray)(*env)->CallObjectMethod(env, jstr, mid, strencode);
    jsize alen = (*env)->GetArrayLength(env, barr);
    jbyte* ba = (*env)->GetByteArrayElements(env, barr, JNI_FALSE);
    if (alen > 0)
    {
        rtn = (char*)malloc(alen + 1);
        memcpy(rtn, ba, alen);
        rtn[alen] = 0;
    }
    (*env)->ReleaseByteArrayElements(env, barr, ba, 0);
    return rtn;
}

int32_t ReceiveWave(void* pAndroidCallBackStat, const char* pWaveSamples, int32_t nBytes)
{
    AndroidCallBackStat* androidStat = (AndroidCallBackStat*)pAndroidCallBackStat;
    JNIEnv* env = androidStat->env;
    jobject obj = androidStat->usingObject;
    jclass jc = (*env)->GetObjectClass(env, obj);
    jmethodID methodID = (*env)->GetMethodID(env, jc, "ReceiveWave", "(Ljava/lang/Object;[BI)I");
    jobject callBackStat = androidStat->pCallBackStat;
    jbyteArray array = (*env)->NewByteArray(env, nBytes);
    (*env)->SetByteArrayRegion(env, array, 0, nBytes, pWaveSamples);
    jint currReceive = (*env)->CallIntMethod(env, obj, methodID, callBackStat, array, (jint)nBytes);
    if(currReceive != 0)
    {
        (*env)->DeleteLocalRef(env, obj);
        (*env)->DeleteLocalRef(env, callBackStat);
        (*env)->DeleteLocalRef(env, array);
        free(pAndroidCallBackStat);
    }
    return (int32_t)currReceive;
}

jlong Java_com_microsoft_speech_ttsclientsdk_Synthesizer_CreateHandler(JNIEnv* env, jclass clazz, jlong phSynthesizerHandle, jstring jstrApiKry)
{
    char * apiKey = jstringTostring(env, jstrApiKry);
    int error = MSTTS_CreateSpeechSynthesizerHandler((MSTTSHANDLE*)&phSynthesizerHandle, apiKey);
    if (error != 0){
        return -1;
    }
    return phSynthesizerHandle;
}

int Java_com_microsoft_speech_ttsclientsdk_Synthesizer_Speak(JNIEnv* env, jclass clazz, jlong phSynthesizerHandle, jstring jstrContent, jint eContentType)
{
    int error = MSTTS_Speak((MSTTSHANDLE)phSynthesizerHandle, jstringTostring(env, jstrContent), (MSTTSContent)eContentType);
    return error;
}

int Java_com_microsoft_speech_ttsclientsdk_Synthesizer_Stop(JNIEnv* env, jclass clazz, jlong phSynthesizerHandle)
{
    int error = MSTTS_Stop((MSTTSHANDLE)phSynthesizerHandle);
    return error;
}

int Java_com_microsoft_speech_ttsclientsdk_Synthesizer_SetOutput(JNIEnv* env, jclass clazz, jlong phSynthesizerHandle, jobject callBackStat, jobject usingObject)
{
    AndroidCallBackStat* tmp = (AndroidCallBackStat*)malloc(sizeof(AndroidCallBackStat));
    tmp->env = env;
    tmp->pCallBackStat = (*env)->NewGlobalRef(env, callBackStat);
    tmp->usingObject = (*env)->NewGlobalRef(env, usingObject);
    int error = MSTTS_SetOutput((MSTTSHANDLE)phSynthesizerHandle, NULL, ReceiveWave, tmp);
    return error;
}

int Java_com_microsoft_speech_ttsclientsdk_Synthesizer_SetVoice(JNIEnv* env, jclass clazz, jlong phSynthesizerHandle, jstring name, jstring lang)
{
    char * voiceName = jstringTostring(env, name);
    char * voiceLang = jstringTostring(env, lang);
    MSTTSVoiceInfo* voiceInfo = (MSTTSVoiceInfo*)malloc(sizeof(MSTTSVoiceInfo));
    voiceInfo->lang = voiceLang;
    voiceInfo->voiceName = voiceName;
    int error = MSTTS_SetVoice((MSTTSHANDLE)phSynthesizerHandle, voiceInfo);
    free(voiceInfo);
    return error;
}

jobject Java_com_microsoft_speech_ttsclientsdk_Synthesizer_GetOutput(JNIEnv* env, jclass clazz, jlong phSynthesizerHandle)
{
    MSTTSWAVEFORMATEX* waveFormat = MSTTS_GetOutputFormat((MSTTSHANDLE)phSynthesizerHandle);

    jclass jc = (*env)->FindClass(env, "com/microsoft/speech/ttsclientsdk/TTSWaveFormat");
    jmethodID formatInit = (*env)->GetMethodID(env, jc, "<init>", "()V");
    jobject obj = (*env)->NewObject(env, jc, formatInit);

    jfieldID fid = (*env)->GetFieldID(env, jc, "wFormatTag", "S");
    (*env)->SetShortField(env, obj, fid, (jshort)waveFormat->wFormatTag);

    fid = (*env)->GetFieldID(env, jc, "nChannels", "S");
    (*env)->SetShortField(env, obj, fid, (jshort)waveFormat->nChannels);

    fid = (*env)->GetFieldID(env, jc, "nSamplesPerSec", "I");
    (*env)->SetIntField(env, obj, fid, (jint)waveFormat->nSamplesPerSec);

    fid = (*env)->GetFieldID(env, jc, "nAvgBytesPerSec", "I");
    (*env)->SetIntField(env, obj, fid, (jint)waveFormat->nAvgBytesPerSec);

    fid = (*env)->GetFieldID(env, jc, "nBlockAlign", "S");
    (*env)->SetShortField(env, obj, fid, (jshort)waveFormat->nBlockAlign);

    fid = (*env)->GetFieldID(env, jc, "wBitsPerSample", "S");
    (*env)->SetShortField(env, obj, fid, (jshort)waveFormat->wBitsPerSample);

    fid = (*env)->GetFieldID(env, jc, "cbSize", "I");
    (*env)->SetIntField(env, obj, fid, (jint)waveFormat->cbSize);
    return obj;
}

void Java_com_microsoft_speech_ttsclientsdk_Synthesizer_CloseHandler(JNIEnv* env, jclass clazz, jlong phSynthesizerHandle)
{
    MSTTS_CloseSynthesizer((MSTTSHANDLE)phSynthesizerHandle);
}
