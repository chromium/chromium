// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.tools.audio_focus_grabber;

import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.media.AudioManager;
import android.media.MediaPlayer;
import android.os.IBinder;
import android.widget.RemoteViews;

import androidx.core.app.NotificationCompat;
import androidx.core.app.NotificationManagerCompat;

import org.chromium.base.IntentUtils;
import org.chromium.base.Log;

/** The listener service, which listens to intents and perform audio focus actions. */
public class AudioFocusGrabberListenerService extends Service {
    private static final String TAG = "AudioFocusGrabber";

    public static final String ACTION_GAIN = "AUDIO_FOCUS_GRABBER_GAIN";
    public static final String ACTION_TRANSIENT_PAUSE = "AUDIO_FOCUS_GRABBER_TRANSIENT_PAUSE";
    public static final String ACTION_TRANSIENT_DUCK = "AUDIO_FOCUS_GRABBER_TRANSIENT_DUCK";
    public static final String ACTION_SHOW_NOTIFICATION = "AUDIO_FOCUS_GRABBER_SHOW_NOTIFICATION";
    public static final String ACTION_HIDE_NOTIFICATION = "AUDIO_FOCUS_GRABBER_HIDE_NOTIFICATION";

    private static final int NOTIFICATION_ID = 1;

    AudioManager mAudioManager;
    MediaPlayer mMediaPlayer;
    boolean mIsDucking;

    @Override
    public void onCreate() {
        super.onCreate();
        mAudioManager =
                (AudioManager) getApplicationContext().getSystemService(Context.AUDIO_SERVICE);
    }

    @Override
    public void onDestroy() {
        hideNotification();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent != null) {
            Log.i(TAG, "received intent: " + intent.getAction());
        } else {
            Log.i(TAG, "received null intent");
            return START_NOT_STICKY;
        }
        processIntent(intent);
        return START_NOT_STICKY;
    }

    private void processIntent(Intent intent) {
        if (mMediaPlayer != null) {
            Log.i(
                    TAG,
                    "There's already a MediaPlayer playing,"
                            + " stopping the existing player and abandon focus");
            releaseAndAbandonAudioFocus();
        }
        String action = intent.getAction();
        if (ACTION_SHOW_NOTIFICATION.equals(action)) {
            showNotification();
        } else if (ACTION_HIDE_NOTIFICATION.equals(action)) {
            hideNotification();
        } else if (ACTION_GAIN.equals(action)) {
            gainFocusAndPlay(AudioManager.AUDIOFOCUS_GAIN);
        } else if (ACTION_TRANSIENT_PAUSE.equals(action)) {
            gainFocusAndPlay(AudioManager.AUDIOFOCUS_GAIN_TRANSIENT);
        } else if (ACTION_TRANSIENT_DUCK.equals(action)) {
            gainFocusAndPlay(AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK);
        } else {
            assert false;
        }
    }

    private void gainFocusAndPlay(int focusType) {
        int result =
                mAudioManager.requestAudioFocus(
                        mOnAudioFocusChangeListener, AudioManager.STREAM_MUSIC, focusType);
        if (result == AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
            playSound();
        } else {
            Log.i(TAG, "cannot request audio focus");
        }
    }

    private void playSound() {
        mMediaPlayer = MediaPlayer.create(getApplicationContext(), R.raw.ping);
        mMediaPlayer.setOnCompletionListener(mOnCompletionListener);
        mMediaPlayer.start();
    }

    private void releaseAndAbandonAudioFocus() {
        mMediaPlayer.release();
        mMediaPlayer = null;
        mAudioManager.abandonAudioFocus(mOnAudioFocusChangeListener);
    }

    MediaPlayer.OnCompletionListener mOnCompletionListener =
            new MediaPlayer.OnCompletionListener() {
                @Override
                public void onCompletion(MediaPlayer mp) {
                    releaseAndAbandonAudioFocus();
                }
            };

    AudioManager.OnAudioFocusChangeListener mOnAudioFocusChangeListener =
            new AudioManager.OnAudioFocusChangeListener() {
                @Override
                public void onAudioFocusChange(int focusChange) {
                    switch (focusChange) {
                        case AudioManager.AUDIOFOCUS_GAIN:
                            if (mIsDucking) {
                                mMediaPlayer.setVolume(1.0f, 1.0f);
                                mIsDucking = false;
                            } else {
                                mMediaPlayer.start();
                            }
                            break;
                        case AudioManager.AUDIOFOCUS_LOSS:
                            mMediaPlayer.stop();
                            mMediaPlayer.release();
                            mMediaPlayer = null;
                            break;
                        case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT:
                            mMediaPlayer.pause();
                            break;
                        case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK:
                            mMediaPlayer.setVolume(0.1f, 0.1f);
                            mIsDucking = true;
                            break;
                        default:
                            break;
                    }
                }
            };

    private void showNotification() {
        RemoteViews view =
                new RemoteViews(
                        this.getPackageName(), R.layout.audio_focus_grabber_notification_bar);
        view.setOnClickPendingIntent(
                R.id.notification_button_gain, createPendingIntent(ACTION_GAIN));
        view.setOnClickPendingIntent(
                R.id.notification_button_transient_pause,
                createPendingIntent(ACTION_TRANSIENT_PAUSE));
        view.setOnClickPendingIntent(
                R.id.notification_button_transient_duck,
                createPendingIntent(ACTION_TRANSIENT_DUCK));
        view.setOnClickPendingIntent(
                R.id.notification_button_hide, createPendingIntent(ACTION_HIDE_NOTIFICATION));

        NotificationManagerCompat manager = NotificationManagerCompat.from(this);
        NotificationCompat.Builder builder =
                new NotificationCompat.Builder(this)
                        .setContent(view)
                        .setSmallIcon(R.drawable.notification_icon);
        manager.notify(NOTIFICATION_ID, builder.build());
    }

    private void hideNotification() {
        NotificationManagerCompat manager = NotificationManagerCompat.from(this);
        manager.cancel(NOTIFICATION_ID);
    }

    private PendingIntent createPendingIntent(String action) {
        Intent i = new Intent(this, AudioFocusGrabberListenerService.class);
        i.setAction(action);
        return PendingIntent.getService(
                this,
                0,
                i,
                PendingIntent.FLAG_CANCEL_CURRENT
                        | IntentUtils.getPendingIntentMutabilityFlag(false));
    }
}
