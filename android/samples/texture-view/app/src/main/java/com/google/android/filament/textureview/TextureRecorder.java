package com.google.android.filament.textureview;

import android.annotation.SuppressLint;
import android.app.Application;
import android.graphics.SurfaceTexture;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaCodecList;
import android.media.MediaFormat;
import android.media.MediaMuxer;
import android.media.MediaRecorder;
import android.os.Environment;
import android.util.Log;
import android.view.Surface;
import android.view.TextureView;

import java.io.File;
import java.io.IOException;

public class TextureRecorder {
    private Surface surface;
    private MediaRecorder recorder;

    // Dimensions
    private static final int WIDTH = 1920;
    private static final int HEIGHT = 1280;
    // 5 Mbps
    private static final int BITRATE = (int) (5 * 1000 * 1000);
    // 30 FPS
    private static final int FRAMERATE = 60;

    private static final String VIDEO_PATH = Environment.getExternalStorageDirectory() + File.separator + "render_video.mp4";


    public TextureRecorder() {
        initRecorder();
    }

    private void initRecorder() {
        recorder = new MediaRecorder();

        recorder.setVideoSource(MediaRecorder.VideoSource.SURFACE);

        recorder.setOutputFormat(MediaRecorder.OutputFormat.MPEG_4);
        recorder.setOutputFile(VIDEO_PATH);
        recorder.setVideoEncodingBitRate(BITRATE);
        recorder.setVideoFrameRate(FRAMERATE);
        recorder.setVideoSize(WIDTH, HEIGHT);
        recorder.setVideoEncoder(MediaRecorder.VideoEncoder.H264);

        recorder.setOnErrorListener(new MediaRecorder.OnErrorListener() {
            @Override
            public void onError(MediaRecorder mr, int what, int extra) {
                Log.e("RANDY", "Error: "  + what);
            }
        });

        recorder.setOnInfoListener(new MediaRecorder.OnInfoListener() {

            @Override
            public void onInfo(MediaRecorder mr, int what, int extra) {
                Log.e("RANDY", "Info: " + what);
            }
        });

        try {
            recorder.prepare();
        } catch (IOException e) {
            return;
        }

        surface = recorder.getSurface();

    }

    public void endRecording() {
        recorder.stop();
        recorder.reset();
        recorder.release();
    }

    public void startRecording() {
        recorder.start();
    }

    public Surface getSurface() {
        return surface;
    }
}
