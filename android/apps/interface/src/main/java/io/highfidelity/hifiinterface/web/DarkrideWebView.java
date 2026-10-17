package io.highfidelity.hifiinterface.web;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.PorterDuff;
import android.graphics.SurfaceTexture;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.util.Log;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.JavascriptInterface;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.FrameLayout;

import androidx.webkit.WebViewCompat;
import androidx.webkit.WebViewFeature;

import org.qtproject.qt5.android.QtNative;

import java.util.Collections;

/**
 * A web page for a web entity: a WebView drawing into a SurfaceTexture, which the engine copies into the entity's
 * texture (interface/src/android/AndroidWebView.cpp, which makes these).
 *
 * The WebView is in the activity's views, so that it lays out, draws and takes input (and later the keyboard) like
 * any view, but its container draws it into the SurfaceTexture's Surface rather than the screen: Flutter's "texture
 * layer" way of showing Android views in a GPU scene. Its container takes no touches: the 3D view under it does.
 *
 * Made and called from the engine's threads; the views are only touched on the UI thread.
 */
public class DarkrideWebView {
    private static final String TAG = "DarkrideWebView";

    private static native void nativeFrameAvailable(long id);
    private static native void nativeWebEvent(long id, String json);

    // Pages talk to scripts through a global EventBridge, as on desktop (interface/resources/html/
    // createGlobalEventBridge.js, over Qt WebChannel there): emitWebEvent(message) goes to Entities.webEventReceived,
    // and the callbacks given to scriptEventReceived.connect get Entities.emitScriptEvent's messages
    private static final String EVENT_BRIDGE_SCRIPT =
            "(function() {" +
            "  if (window.EventBridge && window.EventBridge.darkrideAndroid) { return; }" +
            "  var callbacks = [];" +
            "  window.EventBridge = {" +
            "    darkrideAndroid: true," +
            "    scriptEventReceived: {" +
            "      connect: function(callback) { callbacks.push(callback); }," +
            "      disconnect: function(callback) {" +
            "        var i = callbacks.indexOf(callback); if (i >= 0) { callbacks.splice(i, 1); }" +
            "      }" +
            "    }," +
            "    emitWebEvent: function(message) { DarkrideNative.emitWebEvent(JSON.stringify(message)); }" +
            "  };" +
            "  window.darkrideScriptEvent = function(message) {" +
            "    callbacks.slice().forEach(function(callback) {" +
            "      try { callback(message); } catch (e) { console.error(e); }" +
            "    });" +
            "  };" +
            "})();";

    private final long mId;
    private final Handler mMainHandler = new Handler(Looper.getMainLooper());
    private int mWidth;
    private int mHeight;
    private String mPendingUrl;

    // on the UI thread
    private RedirectLayout mContainer;
    private WebView mWebView;
    private SurfaceTexture mSurfaceTexture;
    private Surface mSurface;
    private boolean mDestroyed;
    private long mTouchDownTime;

    public DarkrideWebView(long id, int width, int height) {
        mId = id;
        mWidth = width;
        mHeight = height;
        mMainHandler.post(this::create);
    }

    @SuppressLint("SetJavaScriptEnabled")
    private void create() {
        Activity activity = QtNative.activity();
        if (activity == null || mDestroyed) {
            return;
        }
        mContainer = new RedirectLayout(activity);
        mWebView = new WebView(activity);
        WebSettings settings = mWebView.getSettings();
        settings.setJavaScriptEnabled(true);
        settings.setDomStorageEnabled(true);
        settings.setMediaPlaybackRequiresUserGesture(false);
        settings.setLoadWithOverviewMode(true);
        settings.setUseWideViewPort(true);
        mWebView.addJavascriptInterface(new EventBridgeInterface(mId), "DarkrideNative");
        if (WebViewFeature.isFeatureSupported(WebViewFeature.DOCUMENT_START_SCRIPT)) {
            WebViewCompat.addDocumentStartJavaScript(mWebView, EVENT_BRIDGE_SCRIPT, Collections.singleton("*"));
            mWebView.setWebViewClient(new WebViewClient());
        } else {
            // an older WebView: the script goes in as each page starts, which can be after the page's own
            mWebView.setWebViewClient(new WebViewClient() {
                @Override
                public void onPageStarted(WebView view, String url, Bitmap favicon) {
                    view.evaluateJavascript(EVENT_BRIDGE_SCRIPT, null);
                }
            });
        }
        mWebView.setBackgroundColor(Color.WHITE);
        mContainer.addView(mWebView, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT));

        ViewGroup content = activity.findViewById(android.R.id.content);
        content.addView(mContainer, new FrameLayout.LayoutParams(mWidth, mHeight));
        if (mSurface != null) {
            mContainer.setSurface(mSurface);
        }
        if (mPendingUrl != null) {
            mWebView.loadUrl(mPendingUrl);
            mPendingUrl = null;
        }
    }

    // On the engine's render thread: frames go to this, which belongs to that thread's GL context
    public void setSurfaceTexture(SurfaceTexture surfaceTexture) {
        surfaceTexture.setDefaultBufferSize(mWidth, mHeight);
        surfaceTexture.setOnFrameAvailableListener(texture -> nativeFrameAvailable(mId), mMainHandler);
        mMainHandler.post(() -> {
            if (mDestroyed) {
                return;
            }
            mSurfaceTexture = surfaceTexture;
            mSurface = new Surface(surfaceTexture);
            if (mContainer != null) {
                mContainer.setSurface(mSurface);
            }
        });
    }

    public void loadUrl(String url) {
        mMainHandler.post(() -> {
            if (mWebView != null) {
                mWebView.loadUrl(url);
            } else {
                mPendingUrl = url;
            }
        });
    }

    // The web entity's pointer, at a position in the page's pixels, as a finger on the WebView
    public void touch(int action, float x, float y) {
        long eventTime = SystemClock.uptimeMillis();
        mMainHandler.post(() -> {
            if (mWebView == null) {
                return;
            }
            if (action == MotionEvent.ACTION_DOWN) {
                mTouchDownTime = eventTime;
                // for the keyboard, when a text field is touched
                mWebView.requestFocus();
            }
            MotionEvent event = MotionEvent.obtain(mTouchDownTime, eventTime, action, x, y, 0);
            event.setSource(InputDevice.SOURCE_TOUCHSCREEN);
            mWebView.dispatchTouchEvent(event);
            event.recycle();
        });
    }

    // A message from a script to the page, as JSON
    public void scriptEvent(String json) {
        mMainHandler.post(() -> {
            if (mWebView != null) {
                mWebView.evaluateJavascript("window.darkrideScriptEvent && window.darkrideScriptEvent(" + json + ")", null);
            }
        });
    }

    public void resize(int width, int height) {
        mMainHandler.post(() -> {
            mWidth = width;
            mHeight = height;
            if (mSurfaceTexture != null) {
                mSurfaceTexture.setDefaultBufferSize(width, height);
            }
            if (mContainer != null) {
                ViewGroup.LayoutParams params = mContainer.getLayoutParams();
                params.width = width;
                params.height = height;
                mContainer.setLayoutParams(params);
            }
        });
    }

    public void destroy() {
        mMainHandler.post(() -> {
            mDestroyed = true;
            if (mContainer != null) {
                mContainer.setSurface(null);
                ViewGroup parent = (ViewGroup) mContainer.getParent();
                if (parent != null) {
                    parent.removeView(mContainer);
                }
                mContainer = null;
            }
            if (mWebView != null) {
                mWebView.destroy();
                mWebView = null;
            }
            if (mSurface != null) {
                mSurface.release();
                mSurface = null;
            }
            // the SurfaceTexture is released by the engine, on its GL thread
            mSurfaceTexture = null;
        });
    }

    // window.DarkrideNative in pages, which EventBridge.emitWebEvent calls (on a WebView thread)
    private static class EventBridgeInterface {
        private final long mId;

        EventBridgeInterface(long id) {
            mId = id;
        }

        @JavascriptInterface
        public void emitWebEvent(String json) {
            nativeWebEvent(mId, json);
        }
    }

    // Draws its child into a Surface instead of the screen
    private static class RedirectLayout extends FrameLayout {
        private Surface mSurface;

        RedirectLayout(Context context) {
            super(context);
            setWillNotDraw(false);
        }

        void setSurface(Surface surface) {
            mSurface = surface;
            invalidate();
        }

        @Override
        public void draw(Canvas screenCanvas) {
            if (mSurface == null || !mSurface.isValid()) {
                return;
            }
            Canvas canvas;
            try {
                canvas = mSurface.lockHardwareCanvas();
            } catch (IllegalStateException | IllegalArgumentException e) {
                Log.w(TAG, "Couldn't draw into the web surface", e);
                return;
            }
            try {
                canvas.drawColor(Color.TRANSPARENT, PorterDuff.Mode.CLEAR);
                super.draw(canvas);
            } finally {
                mSurface.unlockCanvasAndPost(canvas);
            }
        }

        @Override
        public void onDescendantInvalidated(View child, View target) {
            super.onDescendantInvalidated(child, target);
            invalidate();
        }

        @Override
        public boolean dispatchTouchEvent(MotionEvent event) {
            // real touches are for the 3D view under this
            return false;
        }
    }
}
