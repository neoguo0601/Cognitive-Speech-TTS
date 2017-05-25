package com.microsoft.speech.ttsclienttest_android;

import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.text.method.ScrollingMovementMethod;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;

import com.microsoft.speech.ttsclientsdk.ITTSCallback;
import com.microsoft.speech.ttsclientsdk.Synthesizer;
import com.microsoft.speech.ttsclientsdk.TTSWaveFormat;

import java.util.Date;

public class MainActivity extends AppCompatActivity {
    private TextView status;
    private Button btnSynthesize;
    private EditText txtContent;
    private Synthesizer m_syn;
    private AudioTrack audioTrack;
    final int SAMPLE_RATE = 16000;
    private long handle = 0;
    private int result= 0 ;
    boolean getTime = false;
    Date curDate = null;
    Date endDate = null;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        // Example of a call to a native method
        status = (TextView) findViewById(R.id.sample_text);
        status.setText("Synthesis time: 0.0");
        txtContent = (EditText)findViewById(R.id.editText);
        txtContent.setMovementMethod(new ScrollingMovementMethod());
        btnSynthesize = (Button)findViewById(R.id.button);

        //init audioTrack
        audioTrack = new AudioTrack(AudioManager.STREAM_MUSIC, SAMPLE_RATE, AudioFormat.CHANNEL_CONFIGURATION_MONO,
                AudioFormat.ENCODING_PCM_16BIT, AudioTrack.getMinBufferSize(SAMPLE_RATE, AudioFormat.CHANNEL_CONFIGURATION_MONO, AudioFormat.ENCODING_PCM_16BIT), AudioTrack.MODE_STREAM);

        //init Synthesizer
        m_syn = new Synthesizer();
        //set Synthesizer call back
        m_syn.setReceiveWave(new ITTSCallback() {
            public int ReceiveWave(Object callBackStat, final byte[] data, int size) {
                //If it is the first call, generate the synthesis time
                if(getTime == true){
                    endDate = new Date(System.currentTimeMillis());
                    long diff = endDate.getTime() - curDate.getTime();
                    long s = (diff/1000);
                    long ms = (diff - s * 1000);
                    String time = String.valueOf(s) + "." + String.valueOf(ms);
                    status.setText("Synthesis time: " + time);
                    getTime = false;
                }

                //write to audio card
                audioTrack.write(data, 0, size);
                return 0;
            }
        });

        //init speech handle
        handle = m_syn.MSTTS_CreateSpeechSynthesizerHandler(handle, getResources().getString(R.string.api_key));

        if(handle == -1){
            status.setText("Please enter you api key.");
            result = -1;
        }

        //set output
        if(result == 0){
            result = m_syn.MSTTS_SetOutput(handle, null, m_syn);
        }

        //set voice
        if(result == 0){
            result = m_syn.MSTTS_SetVoice(handle, "Microsoft Server Speech Text to Speech Voice (en-US, ZiraRUS)", "en-US");
        }

        btnSynthesize.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if(result == 0){
                    curDate = new Date(System.currentTimeMillis());
                    getTime = true;
                    audioTrack.play();
                    result = m_syn.MSTTS_Speak(handle, txtContent.getText().toString(), 0);
                }
            }
        });
    }
}
