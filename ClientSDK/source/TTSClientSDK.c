// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include<stdlib.h>
#include<string.h>
#include"curl/curl.h"
#include"TTSClientSDK.h"
#include"SKP_Silk_SDK_API.h"

#define SILK_SAMPLESPERFRAME        320
#define SILK_MAXBYTESPERBLOCK       (SILK_SAMPLESPERFRAME * sizeof(uint16_t)) // VBR
#define MAX_BYTES_PER_FRAME         250 // Equals peak bitrate of 100 kbps 

#define WAVE_FORMAT_SIZE            (2 + 2 + 4 + 4 + 2 + 2)
#define WAVE_FORMAT_PCM             1
#define AUDIO_CHANNEL               1
#define AUDIO_BITS                  16
#define AUDIO_SAMPLE_RATE           16000
#define AUDIO_BLOCK_ALIGN           (AUDIO_CHANNEL * (AUDIO_BITS >> 3))
#define AUDIO_BYTE_RATE             (AUDIO_SAMPLE_RATE * AUDIO_BLOCK_ALIGN)
#define TEMP_WAVE_DATA_LENGTH       AUDIO_SAMPLE_RATE * AUDIO_BITS / 2 * 10

typedef enum MSTTSSpeakStatusType
{
	MSTTSAudioSYS_RUNNING,
	MSTTSAudioSYS_STOP
}MSTTSSpeakStatus;

typedef struct MSTTSOUTPUT_TAG
{
	//Call back to output the wave samples, and return <0 for error code and abort speaking.
	LPMSTTS_RECEIVE_WAVESAMPLES_ROUTINE pfWriteBack;  
	//The call back stat for the call back.
	void* pCallBackStat;                              
}MSTTS_OUTPUT;

typedef struct MSTTSHANDLE_TAG
{
	unsigned char* ApiKey;          //The key to accessing the page.
	unsigned char* Token;           //Access the token.
	time_t  timeStamp;              //Record token acquisition time, token timeout will automatically request a new token.
	MSTTSSpeakStatus Speakstatus;   //Status of speak.
	MSTTSVoiceInfo* VoiceInfo;      //Voice info.
	MSTTS_OUTPUT* outputCallback;   //Output call back.
	MSTTSWAVEFORMATEX* waveFormat;  //output wave format
	void* hDecoder;                 //silk decode handle
}MSTTS_HANDLE;

typedef struct HTTPRESPONSECONTENTHANDLE_TAG
{
	unsigned char* buffer;          //Silk buffer.
	size_t bufferSize;              //Silk buffer size.
	size_t offset;                  //The offset of the wave samples that have been output.
	uint32_t* waveSamplesSize;      //Wave samples size.
	MSTTSSpeakStatus* Speakstatus;  //Status of speak.
	MSTTS_OUTPUT* outputCallback;   //Output call back.
	void* hDecoder;                 //silk decode handle
} HTTPRESPONSECONTENT_HANDLE;

//
//Silk decode source
//
static int silk_decode_frame(
	void* hDecoder, 
	const SKP_uint8* inData, 
	SKP_int nBytesIn, 
	SKP_int16* outData, 
	size_t* nBytesOut)
{
	SKP_int16 len;
	int       tot_len = 0;
	SKP_int   ret;
	SKP_SILK_SDK_DecControlStruct DecControl;
	SKP_int   decodedBytes;

	DecControl.API_sampleRate = AUDIO_SAMPLE_RATE;

	// Loop through frames in packet
	do
	{
		// Decode one frame
		ret = SKP_Silk_SDK_Decode(hDecoder, &DecControl, 0, inData, nBytesIn, outData, &len);
		if (ret)
		{
			break;
		}

		outData += len;
		tot_len += len;

		decodedBytes = DecControl.frameSize * DecControl.framesPerPacket;
		if (nBytesIn >= decodedBytes)
		{
			inData += decodedBytes;
			nBytesIn -= decodedBytes;
			DecControl.moreInternalDecoderFrames = 1;
		}

	} while (DecControl.moreInternalDecoderFrames);

	// Convert short array count to byte array count
	*nBytesOut = (size_t)tot_len * sizeof(SKP_int16);

	return ret;
}

static int initdecoder(void** hDecoder)
{
	SKP_int ret;
	if (!*hDecoder)
	{
		SKP_int32 decsize = 0;
		ret = SKP_Silk_SDK_Get_Decoder_Size(&decsize);
		if (ret)
		{
			return ret;
		}

		*hDecoder = malloc((size_t)decsize);
		if (!*hDecoder)
		{
			return -1;
		}
	}

	return 0;
}

static void audio_decoder_uninitialize(void** hDecoder)
{
	if (*hDecoder)
	{
		free(*hDecoder);
		*hDecoder = NULL;
	}
}


/*
* Call back to get the token
* Handle the responce to get the token
* Parameters:
*   data: The response data.
*   size: The size of the response data block.
*   nmemb: Number of response data blocks.
*   Token: The address of save the token.
* Return value:
*   processed data size
*/
static size_t HandleTokenData(void *data, size_t size, size_t nmemb, unsigned char** Token)
{
	unsigned char* TokenBuf = malloc(size*nmemb + 1);
	if (TokenBuf)
	{
		memset(TokenBuf, 0, size*nmemb + 1);
		strncpy(TokenBuf, data, size*nmemb);
		*Token = TokenBuf;
	}

	return size*nmemb;
}

/*
* Call back to get the wave samples
* Handle the responce to get the wave samples
* Parameters:
*   data: The response data.
*   size: The size of the response data block.
*   nmemb: Number of response data blocks.
*   responseContent: responce, type of HTTPRESPONSECONTENT_HANDLE.
* Return value:
*   processed data size
*/
static size_t HandleWaveSamples(void *ptr, size_t size, size_t nmemb, void *responseContent)
{
	HTTPRESPONSECONTENT_HANDLE *response = (HTTPRESPONSECONTENT_HANDLE *)responseContent;
	size_t		decodedBytes = 0;
	size_t      nBytes;

	if ((response != NULL) &&
		(ptr != NULL) &&
		(size * nmemb > 0))
	{
		//executed on first receipt
		if (!response->offset)
		{
			*response->Speakstatus = MSTTSAudioSYS_RUNNING;
			*response->waveSamplesSize = 0;
			if (SKP_Silk_SDK_InitDecoder(response->hDecoder))
			{
				return 0;
			}
		}

		//stop handle wave samples
		if (*response->Speakstatus == MSTTSAudioSYS_STOP)
		{
			return 0;
		}

		//copy the silk data to buffer
		void* newBuffer = realloc(response->buffer, response->bufferSize + (size * nmemb));
		if (newBuffer != NULL)
		{
			response->buffer = newBuffer;
			memcpy(response->buffer + response->bufferSize, ptr, size * nmemb);
			response->bufferSize += size * nmemb;
		}
		else
		{
			return 0;
		}

		//decode silk
		unsigned char *waveOutput = malloc(TEMP_WAVE_DATA_LENGTH);
		if (waveOutput)
		{
			memset(waveOutput, 0, TEMP_WAVE_DATA_LENGTH);

			uint16_t len = *(uint16_t*)(response->buffer + response->offset);
			while (response->offset + len + sizeof(uint16_t) <= response->bufferSize 
				&& TEMP_WAVE_DATA_LENGTH - decodedBytes > SILK_MAXBYTESPERBLOCK)
			{
				nBytes = TEMP_WAVE_DATA_LENGTH - decodedBytes;
				if (silk_decode_frame(
					response->hDecoder,
					response->buffer + response->offset + sizeof(uint16_t),
					len,
					(short*)(waveOutput + decodedBytes),
					&nBytes))
				{
					free(waveOutput);
					return 0;
				}

				response->offset += (sizeof(uint16_t) + len);
				decodedBytes += nBytes;

				//the first two bytes of silk are the data length
				len = *(uint16_t*)(response->buffer + response->offset);
			}

			*response->waveSamplesSize += decodedBytes;

			//callback WriteBack
			if (response->outputCallback->pfWriteBack)
			{
				if (response->outputCallback->pfWriteBack(
					response->outputCallback->pCallBackStat, 
					waveOutput, 
					decodedBytes) != 0)
				{
					free(waveOutput);
					return 0;
				}
			}
			free(waveOutput);
		}
		else
		{
			return 0;
		}
	}

	return size * nmemb;
}

/*
* Get the token by api key
* Parameters:
*   ApiKey: Request the token's api key.
*   KeyValue: The address of save the token.
* Return value:
*  MSTTS_RESULT
*/
MSTTS_RESULT GetToken(const unsigned char* ApiKey, unsigned char** KeyValue)
{
	//Request the URL of the token
	const char* URL = "https://api.cognitive.microsoft.com/sts/v1.0/issueToken";
	const char* ApiKeyHeaderName = "Ocp-Apim-Subscription-Key:";
	
	unsigned char* apiKeyHeader = NULL;
	struct curl_slist *headers = NULL;
	CURL *curl = NULL;
	long httpStatusCode = 0;
	MSTTS_RESULT result = MSTTS_OK;

	if (ApiKey == NULL || KeyValue == NULL)
	{
		return MSTTS_INVALID_ARG;
	}

	//set api key header
	apiKeyHeader = malloc(strlen(ApiKeyHeaderName) + strlen(ApiKey) + 1);
	if (!apiKeyHeader)
	{
		result = MSTTS_MALLOC_FAILED;
	}
	else
	{
		memset(apiKeyHeader, 0, strlen(ApiKeyHeaderName) + strlen(ApiKey) + 1);
		strcat(apiKeyHeader, ApiKeyHeaderName);
		strcat(apiKeyHeader, ApiKey);
	}

	if (result == MSTTS_OK)
	{
		//add header 
		headers = curl_slist_append(headers, apiKeyHeader);
		headers = curl_slist_append(headers, "Content-Length:0");
		if (!headers)
		{
			free(apiKeyHeader);
			result = MSTTS_GET_HEADER_ERROR;
		}
	}

	if (result == MSTTS_OK) 
	{
		//CURL request
		if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
		{
			result = MSTTS_HTTP_INIT_ERROR;
		}
		else if (!(curl = curl_easy_init()))
		{
			result = MSTTS_HTTP_INIT_ERROR;
		}
		else if (curl_easy_setopt(curl, CURLOPT_WRITEDATA, KeyValue) != CURLE_OK)
		{
			result = MSTTS_HTTP_SETOPT_ERROR;
		}
		else if (curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, HandleTokenData) != CURLE_OK)
		{
			result = MSTTS_HTTP_SETOPT_ERROR;
		}
		else if (curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers) != CURLE_OK)
		{
			result = MSTTS_HTTP_SETOPT_ERROR;
		}
		else if (curl_easy_setopt(curl, CURLOPT_URL, URL) != CURLE_OK)
		{
			result = MSTTS_HTTP_SETOPT_ERROR;
		}
		else if (curl_easy_setopt(curl, CURLOPT_POST, 1) != CURLE_OK)
		{
			result = MSTTS_HTTP_SETOPT_ERROR;
		}
		else if (curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1) != CURLE_OK)
		{
			result = MSTTS_HTTP_SETOPT_ERROR;
		}
#ifdef CURL_VERBOSE
		else if (curl_easy_setopt(curl, CURLOPT_VERBOSE, 1) != CURLE_OK)
		{
			result = MSTTS_HTTP_SETOPT_ERROR;
		}
#endif // CURL_VERBOSE
		else if (curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0) != CURLE_OK)
		{
			result = MSTTS_HTTP_SETOPT_ERROR;
		}
		else if (curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0) != CURLE_OK)
		{
			result = MSTTS_HTTP_SETOPT_ERROR;
		}
		else if (curl_easy_perform(curl) != CURLE_OK)
		{
			result = MSTTS_HTTP_PERFORM_BREAK;
		}
		else if ((curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatusCode) != CURLE_OK) 
			|| httpStatusCode != 200)
		{
			result = MSTTS_HTTP_GETINFO_ERROR;
		}
		
		free(apiKeyHeader);
		curl_slist_free_all(headers);
		curl_easy_cleanup(curl);
		curl_global_cleanup();
	}

	return result;
}

/*
* Verify that Token is valid
* Token will expire every ten minutes,
* and if the requested token is more than 9 minutes,
* it will be requested again.
* Parameters:
*   hSynthesizerHandle: The handle of the synthesizer instance.
* Return value:
*  MSTTS_RESULT
*/
MSTTS_RESULT CheckToken(MSTTSHANDLE hSynthesizerHandle)
{
	MSTTS_HANDLE *SynthesizerHandle = (MSTTS_HANDLE *)hSynthesizerHandle;
	MSTTS_RESULT result = MSTTS_OK;

	if (hSynthesizerHandle == NULL)
	{
		return MSTTS_INVALID_ARG;
	}

	//if more than 9 minutes, need to reopen the access token
	time_t time_now;
	time(&time_now);
	double cost = difftime(time_now, SynthesizerHandle->timeStamp);
	if (cost > 9 * 60)
	{
		free(SynthesizerHandle->Token);
		result = GetToken(SynthesizerHandle->ApiKey, &SynthesizerHandle->Token);
		if (result == MSTTS_OK)
		{
			time(&SynthesizerHandle->timeStamp);
		}
	}

	return result;
}

/*
* Generate SSML for synthesis
* Parameters:
*   hSynthesizerHandle: The handle of the synthesizer instance.
*   pszContent: Text.
*   eContentType: Type of text or SSML.
*   body: Generates the SSML address.
* Return value:
*  MSTTS_RESULT
*/
MSTTS_RESULT GetSSML(
	MSTTSHANDLE hSynthesizerHandle, 
	const char* pszContent, 
	enum MSTTSContentType eContentType, 
	unsigned char** body)
{
	MSTTS_HANDLE *SynthesizerHandle = (MSTTS_HANDLE *)hSynthesizerHandle;
	const unsigned char* SSMLFormat = "<speak version='1.0' xml:lang='%s'><voice xml:lang='%s' name='%s'>%s</voice></speak>";
	MSTTS_RESULT result = MSTTS_OK;

	if (SynthesizerHandle == NULL || pszContent == NULL || body == NULL)
	{
		return MSTTS_INVALID_ARG;
	}

	if (eContentType == MSTTSContentType_SSML)
	{
		size_t len = strlen(pszContent);

		*body = malloc(len + 1);
		if (!*body)
		{
			result = MSTTS_MALLOC_FAILED;
		}
		else
		{
			memset(*body, 0, len + 1);
			strcpy(*body, pszContent);
		}
	}
	else
	{
		size_t len = strlen(SSMLFormat) +
			strlen(SynthesizerHandle->VoiceInfo->lang) +
			strlen(SynthesizerHandle->VoiceInfo->lang) +
			strlen(SynthesizerHandle->VoiceInfo->voiceName) +
			strlen(pszContent);

		*body = malloc(len + 1);
		if (!*body)
		{
			result = MSTTS_MALLOC_FAILED;
		}
		else
		{
			memset(*body, 0, len + 1);
			snprintf(*body, len + 1, SSMLFormat,
				SynthesizerHandle->VoiceInfo->lang,
				SynthesizerHandle->VoiceInfo->lang,
				SynthesizerHandle->VoiceInfo->voiceName,
				pszContent);
		}
	}

	return MSTTS_OK;
}

MSTTSVoiceInfo* InitMSTTSVoiceHandle()
{
	const unsigned char* cDefaultVoiceName = "Microsoft Server Speech Text to Speech Voice (zh-CN, HuihuiRUS)";
	const unsigned char* cDefaultLang = "zh-CN";
	MSTTSVoiceInfo* MSTTSVoiceHandle = NULL;
	unsigned char* lang = NULL;
	unsigned char* voiceName = NULL;
	MSTTS_RESULT result = MSTTS_OK;

	MSTTSVoiceHandle = (MSTTSVoiceInfo*)malloc(sizeof(MSTTSVoiceInfo));
	if(!MSTTSVoiceHandle)
	{
		result = MSTTS_MALLOC_FAILED;
	}

	if(result == MSTTS_OK)
	{
		lang = malloc(strlen(cDefaultLang) + 1);
		if (!lang) 
		{
			free(MSTTSVoiceHandle);
			result = MSTTS_MALLOC_FAILED;
		}
	}
	
	if (result == MSTTS_OK)
	{
		voiceName = malloc(strlen(cDefaultVoiceName) + 1);
		if (!voiceName)
		{
			free(MSTTSVoiceHandle);
			free(lang);
			result = MSTTS_MALLOC_FAILED;
		}
	}

	if (result == MSTTS_OK)
	{
		memset(lang, 0, strlen(cDefaultLang) + 1);
		strcpy(lang, cDefaultLang);
		MSTTSVoiceHandle->lang = lang;

		memset(voiceName, 0, strlen(cDefaultVoiceName) + 1);
		strcpy(voiceName, cDefaultVoiceName);
		MSTTSVoiceHandle->voiceName = voiceName;
	}

	return MSTTSVoiceHandle;
}

void DestroyMSTTSVoiceHandle(MSTTSVoiceInfo* MSTTSVoiceHandle)
{
	if (MSTTSVoiceHandle)
	{
		if (MSTTSVoiceHandle->voiceName)
		{
			free(MSTTSVoiceHandle->voiceName);
		}
		if (MSTTSVoiceHandle->lang)
		{
			free(MSTTSVoiceHandle->lang);
		}
		free(MSTTSVoiceHandle);
	}
}

/*
* Create a synthesizer instance.
* Parameters:
*   hSynthesizerHandle: The handle of the synthesizer instance.
*   MSTTSApiKey: Request the token's api key.
* Return value:
*  MSTTS_RESULT
*/
MSTTS_RESULT MSTTS_CreateSpeechSynthesizerHandler(MSTTSHANDLE* phSynthesizerHandle, const unsigned char* MSTTSApiKey)
{
	unsigned char* token = NULL;
	MSTTSVoiceInfo* MSTTSVoiceHandle = NULL;
	MSTTSWAVEFORMATEX* waveFormat = NULL;
	MSTTS_HANDLE* MSTTShandle = NULL;
	unsigned char* ApiKey = NULL;
	MSTTS_RESULT result = MSTTS_OK;

	if (MSTTSApiKey == NULL || phSynthesizerHandle == NULL)
	{
		return MSTTS_INVALID_ARG;
	}

	MSTTSVoiceHandle = InitMSTTSVoiceHandle();
	if (!MSTTSVoiceHandle)
	{
		result = MSTTS_MALLOC_FAILED;
	}

	if (result == MSTTS_OK)
	{
		waveFormat = (MSTTSWAVEFORMATEX*)malloc(sizeof(MSTTSWAVEFORMATEX));
		if (!waveFormat)
		{
			DestroyMSTTSVoiceHandle(MSTTSVoiceHandle);
			result = MSTTS_MALLOC_FAILED;
		}
		else
		{
			waveFormat->wFormatTag = WAVE_FORMAT_PCM;
			waveFormat->nChannels = AUDIO_CHANNEL;
			waveFormat->nSamplesPerSec = AUDIO_SAMPLE_RATE;
			waveFormat->wBitsPerSample = AUDIO_BITS;
			waveFormat->nAvgBytesPerSec = AUDIO_BYTE_RATE;
			waveFormat->nBlockAlign = AUDIO_BLOCK_ALIGN;
			waveFormat->cbSize = 0;
		}
	}
	
	if (result == MSTTS_OK) 
	{
		if (GetToken(MSTTSApiKey, &token) != MSTTS_OK)
		{
			DestroyMSTTSVoiceHandle(MSTTSVoiceHandle);
			free(waveFormat);
			result = MSTTS_GET_TOKEN_FAILED;
		}
	}
	
	if (result == MSTTS_OK) 
	{
		ApiKey = malloc(strlen(MSTTSApiKey) + 1);
		if (!ApiKey)
		{
			DestroyMSTTSVoiceHandle(MSTTSVoiceHandle);
			free(waveFormat);
			free(token);
			result = MSTTS_MALLOC_FAILED;
		}
		else 
		{
			memset(ApiKey, 0, strlen(MSTTSApiKey) + 1);
			strcpy(ApiKey, MSTTSApiKey);
		}
	}

	if (result == MSTTS_OK) 
	{
		MSTTShandle = (MSTTS_HANDLE*)malloc(sizeof(MSTTS_HANDLE));
		if (!MSTTShandle)
		{
			DestroyMSTTSVoiceHandle(MSTTSVoiceHandle);
			free(waveFormat);
			free(token);
			free(ApiKey);
			result = MSTTS_MALLOC_FAILED;
		}
		else
		{
			MSTTShandle->ApiKey = ApiKey;
			MSTTShandle->Token = token;
			time(&MSTTShandle->timeStamp);
			MSTTShandle->Speakstatus = MSTTSAudioSYS_STOP;
			MSTTShandle->VoiceInfo = MSTTSVoiceHandle;
			MSTTShandle->outputCallback = NULL;
			MSTTShandle->waveFormat = waveFormat;
			MSTTShandle->hDecoder = NULL;
			*phSynthesizerHandle = MSTTShandle;
		}
	}
	
	return result;
}

/*
* Do text rendering.
* Parameters:
*   hSynthesizerHandle: The handle of the synthesizer instance.
*   pszContent: Text.
*   eContentType: Typr of SSML or text.
* Return value:
*  MSTTS_RESULT
*/
MSTTS_RESULT MSTTS_Speak(MSTTSHANDLE hSynthesizerHandle, const char* pszContent, enum MSTTSContentType eContentType)
{
	MSTTS_HANDLE *SynthesizerHandle = (MSTTS_HANDLE *)hSynthesizerHandle;
	const char* speechURL = "https://speech.platform.bing.com/synthesize";
	const char* tokenHeaderName = "Authorization:Bearer ";
	unsigned char* tokenHeader = NULL;
	struct curl_slist *headers = NULL;
	unsigned char* body = NULL;
	HTTPRESPONSECONTENT_HANDLE *responsecontent = NULL;
	CURL *curl = NULL;
	long httpStatusCode = 0;
	MSTTS_RESULT result = MSTTS_OK;

	if (SynthesizerHandle == NULL || pszContent == NULL)
	{
		return MSTTS_INVALID_ARG;
	}

	if (SynthesizerHandle->outputCallback == NULL || SynthesizerHandle->outputCallback->pfWriteBack == NULL)
	{
		return MSTTS_CALLBACK_HAVE_NOT_SET;
	}

	//init silk
	if (initdecoder(&SynthesizerHandle->hDecoder) != 0) {
		result = MSTTS_SILK_INIT_ERROR;
	}

	//check token
	if (result == MSTTS_OK) 
	{
		if (CheckToken(hSynthesizerHandle) != MSTTS_OK)
		{
			result = MSTTS_GET_TOKEN_FAILED;
		}
	}
	
	//set token header
	if (result == MSTTS_OK)
	{
		tokenHeader = malloc(strlen(tokenHeaderName) + strlen(SynthesizerHandle->Token) + 1);
		if (!tokenHeader)
		{
			result = MSTTS_MALLOC_FAILED;
		}
		else
		{
			memset(tokenHeader, 0, strlen(tokenHeaderName) + strlen(SynthesizerHandle->Token) + 1);
			strcat(tokenHeader, tokenHeaderName);
			strcat(tokenHeader, SynthesizerHandle->Token);
		}
	}
	
	//set SSML
	if (result == MSTTS_OK)
	{
		if (GetSSML(SynthesizerHandle, pszContent, eContentType, &body) != MSTTS_OK)
		{
			free(tokenHeader);
			result = MSTTS_GET_SSML_ERROR;
		}
	}

	//set response content handle
	if (result == MSTTS_OK)
	{
		responsecontent = (HTTPRESPONSECONTENT_HANDLE*)malloc(sizeof(HTTPRESPONSECONTENT_HANDLE));
		if (!responsecontent)
		{
			free(tokenHeader);
			free(body);
			result = MSTTS_GET_HEADER_ERROR;
		}
		else
		{
			responsecontent->buffer = NULL;
			responsecontent->bufferSize = 0;
			responsecontent->offset = 0;
			responsecontent->waveSamplesSize = &SynthesizerHandle->waveFormat->cbSize;
			responsecontent->Speakstatus = &SynthesizerHandle->Speakstatus;
			responsecontent->outputCallback = SynthesizerHandle->outputCallback;
			responsecontent->hDecoder = SynthesizerHandle->hDecoder;
		}
	}

	//add header
	if (result == MSTTS_OK) 
	{
		headers = curl_slist_append(headers, "Content-type:application/ssml+xml");
		headers = curl_slist_append(headers, "X-Microsoft-OutputFormat:raw-16khz-16bit-mono-truesilk");
		headers = curl_slist_append(headers, "X-Search-AppId:07D3234E49CE426DAA29772419F436CA");
		headers = curl_slist_append(headers, "X-Search-ClientID:1ECFAE91408841A480F00935DC390960");
		headers = curl_slist_append(headers, "User-Agent:TTSForPython");
		headers = curl_slist_append(headers, tokenHeader);
		if (!headers) 
		{
			free(tokenHeader);
			free(body);
			free(responsecontent);
			result = MSTTS_GET_HEADER_ERROR;
		}
	}

	if (result == MSTTS_OK) 
	{
		if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
		{
			result = MSTTS_HTTP_INIT_ERROR;
		}
		else if (!(curl = curl_easy_init())) 
		{
			result = MSTTS_HTTP_INIT_ERROR;
		}
		else if (curl_easy_setopt(curl, CURLOPT_WRITEDATA, responsecontent) != CURLE_OK)
		{
			result = MSTTS_HTTP_SETOPT_ERROR;
		}
		else if (curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, HandleWaveSamples) != CURLE_OK)
		{
			result = MSTTS_HTTP_SETOPT_ERROR;
		}
		else if (curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers) != CURLE_OK)
		{
			result = MSTTS_HTTP_SETOPT_ERROR;
		}
		else if (curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body) != CURLE_OK)
		{
			result = MSTTS_HTTP_SETOPT_ERROR;
		}
		else if (curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(body)) != CURLE_OK)
		{
			result = MSTTS_HTTP_SETOPT_ERROR;
		}
		else if (curl_easy_setopt(curl, CURLOPT_URL, speechURL) != CURLE_OK)
		{
			result = MSTTS_HTTP_SETOPT_ERROR;
		}
		else if (curl_easy_setopt(curl, CURLOPT_POST, 1) != CURLE_OK)
		{
			result = MSTTS_HTTP_SETOPT_ERROR;
		}
		else if (curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1) != CURLE_OK)
		{
			result = MSTTS_HTTP_SETOPT_ERROR;
		}
#ifdef CURL_VERBOSE
		else if (curl_easy_setopt(curl, CURLOPT_VERBOSE, 1) != CURLE_OK)
		{
			result = MSTTS_HTTP_SETOPT_ERROR;
		}
#endif
		else if (curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0) != CURLE_OK)
		{
			result = MSTTS_HTTP_SETOPT_ERROR;
		}
		else if (curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0) != CURLE_OK)
		{
			result = MSTTS_HTTP_SETOPT_ERROR;
		}
		else if (curl_easy_perform(curl) != CURLE_OK)
		{
			result = MSTTS_HTTP_PERFORM_BREAK;
		}
		else if ((curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatusCode) != CURLE_OK) 
			|| httpStatusCode != 200)
		{
			result = MSTTS_HTTP_GETINFO_ERROR;
		}

		if (responsecontent->buffer)
		{
			free(responsecontent->buffer);
		}
		free(responsecontent);
		free(body);
		free(tokenHeader);
		curl_slist_free_all(headers);
		curl_easy_cleanup(curl);
		curl_global_cleanup();
	}
	audio_decoder_uninitialize(&SynthesizerHandle->hDecoder);

	return result;
}

/*
* Stop speaking
* Parameters:
*   hSynthesizerHandle: The handle of the synthesizer instance.
* Return value:
*  MSTTS_RESULT
*/
MSTTS_RESULT MSTTS_Stop(MSTTSHANDLE hSynthesizerHandle)
{
	MSTTS_HANDLE *SynthesizerHandle = (MSTTS_HANDLE *)hSynthesizerHandle;

	if (SynthesizerHandle == NULL)
	{
		return MSTTS_INVALID_ARG;
	}

	SynthesizerHandle->Speakstatus = MSTTSAudioSYS_STOP;

	return MSTTS_OK;
}

/*
* Set the default voice of the current synthesizer instance,
* it will be used to speak the plain text,
* and ssml without voice name tags.
* Parameters:
*   hSynthesizerHandle: The handle of the synthesizer instance.
*   pVoiceInfo: This is the voice information in voice token file.
* Return value:
*  MSTTS_RESULT
*/
MSTTS_RESULT MSTTS_SetVoice(MSTTSHANDLE hSynthesizerHandle, const MSTTSVoiceInfo* pVoiceInfo)
{
	MSTTS_HANDLE *SynthesizerHandle = (MSTTS_HANDLE *)hSynthesizerHandle;
	unsigned char* newVoiceName = NULL;
	unsigned char* newLang = NULL;
	MSTTS_RESULT result = MSTTS_OK;

	if (SynthesizerHandle == NULL || pVoiceInfo->voiceName == NULL || pVoiceInfo->lang == NULL)
	{
		return MSTTS_INVALID_ARG;
	}

	unsigned char* voiceName = SynthesizerHandle->VoiceInfo->voiceName;
	unsigned char* lang = SynthesizerHandle->VoiceInfo->lang;

	size_t voiceNameLen = strlen(pVoiceInfo->voiceName);
	size_t langLen = strlen(pVoiceInfo->lang);

	if (!(newVoiceName = malloc(voiceNameLen + 1)))
	{
		result = MSTTS_MALLOC_FAILED;
	}
	else if (!(newLang = malloc(langLen + 1)))
	{
		free(newVoiceName);
		result = MSTTS_MALLOC_FAILED;
	}
	else 
	{
		memset(newVoiceName, 0, voiceNameLen + 1);
		strcpy(newVoiceName, pVoiceInfo->voiceName);

		memset(newLang, 0, langLen + 1);
		strcpy(newLang, pVoiceInfo->lang);

		SynthesizerHandle->VoiceInfo->voiceName = newVoiceName;
		SynthesizerHandle->VoiceInfo->lang = newLang;

		free(voiceName);
		free(lang);
	}

	return result;
}

/*
* Set the output format for the synthesizer instance.
* All voices loaded by this instance will use the output format.
* Now, only supports raw-16khz-16bit-mono-truesilk,
* setting pWaveFormat is not implemented, just provied an interface for ecpansion.
* Parameters:
*  hSynthesizerHandle: the handle of the synthesizer instance
*  pWaveFormat: wave format to be set, if set to NULL, use TTS engine's default format.
*  pfWriteBack: Call back to output the wave samples, and return <0 for error code and abort speaking.
*  void* pCallBackStat: The call back stat for the call back.
* Return value:
*  MSTTS_RESULT
*/
MSTTS_RESULT MSTTS_SetOutput(
	MSTTSHANDLE hSynthesizerHandle, 
	const MSTTSWAVEFORMATEX* pWaveFormat, 
	LPMSTTS_RECEIVE_WAVESAMPLES_ROUTINE pfWriteBack, 
	void* pCallBackStat)
{
	MSTTS_HANDLE *SynthesizerHandle = (MSTTS_HANDLE *)hSynthesizerHandle;
	MSTTS_OUTPUT* outputCallback = NULL;
	MSTTS_RESULT result = MSTTS_OK;

	if (SynthesizerHandle == NULL || pfWriteBack == NULL)
	{
		return MSTTS_INVALID_ARG;
	}

	if (SynthesizerHandle->outputCallback)
	{
		SynthesizerHandle->outputCallback->pCallBackStat = pCallBackStat;
		SynthesizerHandle->outputCallback->pfWriteBack = pfWriteBack;
	}
	else if (outputCallback = (MSTTS_OUTPUT*)malloc(sizeof(MSTTS_OUTPUT)))
	{
		outputCallback->pCallBackStat = pCallBackStat;
		outputCallback->pfWriteBack = pfWriteBack;
		SynthesizerHandle->outputCallback = outputCallback;
	}
	else
	{
		result = MSTTS_MALLOC_FAILED;
	}

	return result;
}

/*
* Get the current synthesizer output format.
* Now only supports raw-16khz-16bit-mono format.
* Parameters:
*  hSynthesizerHandle: the handle of the synthesizer instance.
* Return value:
*  MSTTS_RESULT
*/
const MSTTSWAVEFORMATEX* MSTTS_GetOutputFormat(MSTTSHANDLE hSynthesizerHandle)
{
	MSTTS_HANDLE *SynthesizerHandle = (MSTTS_HANDLE *)hSynthesizerHandle;

	if (SynthesizerHandle == NULL)
	{
		return NULL;
	}

	return (const MSTTSWAVEFORMATEX*)SynthesizerHandle->waveFormat;
}

/*
* Stop speaking and destroy the synthesizer.
* Parameters:
*  hSynthesizerHandle: the handle of the synthesizer instance.
*/
void MSTTS_CloseSynthesizer(MSTTSHANDLE hSynthesizerHandle)
{
	MSTTS_HANDLE *SynthesizerHandle = (MSTTS_HANDLE *)hSynthesizerHandle;

	if (SynthesizerHandle)
	{
		MSTTS_Stop(SynthesizerHandle);

		DestroyMSTTSVoiceHandle((MSTTSVoiceInfo *)SynthesizerHandle->VoiceInfo);

		if (SynthesizerHandle->outputCallback)
		{
			free(SynthesizerHandle->outputCallback);
		}
		if (SynthesizerHandle->waveFormat)
		{
			free(SynthesizerHandle->waveFormat);
		}
		if (SynthesizerHandle->ApiKey)
		{
			free(SynthesizerHandle->ApiKey);
		}
		if (SynthesizerHandle->Token)
		{
			free(SynthesizerHandle->Token);
		}
		free(SynthesizerHandle);
	}
	
	return;
}
