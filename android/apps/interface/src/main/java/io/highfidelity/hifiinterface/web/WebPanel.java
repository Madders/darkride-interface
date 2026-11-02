package io.highfidelity.hifiinterface.web;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.Dialog;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.graphics.drawable.ColorDrawable;
import android.net.Uri;
import android.util.Log;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.webkit.RenderProcessGoneDetail;
import android.webkit.WebChromeClient;
import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.Toast;

import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsControllerCompat;
import androidx.webkit.JavaScriptReplyProxy;
import androidx.webkit.WebViewAssetLoader;
import androidx.webkit.WebViewCompat;
import androidx.webkit.WebViewFeature;

import org.qtproject.qt5.android.QtNative;

import java.lang.ref.WeakReference;
import java.util.Collections;
import java.util.Set;

import io.highfidelity.hifiinterface.R;
import io.highfidelity.hifiinterface.theme.Theme;
import io.highfidelity.hifiinterface.theme.ThemeManager;

/**
 * A web page full-screen over the world, with a close box: Darkride chat's web client, and the radio's page. It's a
 * dialog of its own, so the world keeps running under it and the keyboard pushes the page up (the world's window takes
 * no part in the keyboard). One at a time; the engine opens and closes them (interface/src/android/PhonePanels.cpp).
 *
 * Chat signs in as the Darkride account, as the VR tablet's chat page does (interface/src/android/AndroidChatPage.cpp):
 * the page gets an EventBridge, and what it sends reaches the engine only from the chat server's own origin and its
 * main frame (addWebMessageListener's origin rule, and checked again on each message), so a frame in it or a site it
 * leads to has no bridge; the engine's answers, the sign-in token among them, go back only to the frame that asked.
 * The page itself stays on that origin: other links open in the phone's browser.
 *
 * Opened and closed from the engine's threads; the views are only touched on the UI thread.
 */
public class WebPanel {
    private static final String TAG = "WebPanel";
    private static final String CHAT = "Chat";

    // the page's message to the engine (AndroidChatPage), as JSON
    private static native void nativeChatMessage(String json);
    // the panel was closed (its close box, the back key, or the engine)
    private static native void nativeClosed(String name);

    // EventBridge for the chat page, as the tablet gives it (emitWebEvent, scriptEventReceived.connect), over the
    // object addWebMessageListener puts in the chat server's pages only
    private static final String CHAT_BRIDGE_OBJECT = "DarkrideChatBridge";
    private static final String CHAT_BRIDGE_SCRIPT =
            "(function() {" +
            "  if (window.top !== window) { return; }" +
            "  var callbacks = [];" +
            "  var listening = false;" +
            "  function bridge() {" +
            "    var port = window." + CHAT_BRIDGE_OBJECT + ";" +
            "    if (port && !listening) {" +
            "      listening = true;" +
            "      port.addEventListener('message', function(event) {" +
            "        callbacks.slice().forEach(function(callback) {" +
            "          try { callback(event.data); } catch (e) { console.error(e); }" +
            "        });" +
            "      });" +
            "    }" +
            "    return port;" +
            "  }" +
            "  bridge();" +
            "  window.EventBridge = {" +
            "    scriptEventReceived: {" +
            "      connect: function(callback) { callbacks.push(callback); bridge(); }," +
            "      disconnect: function(callback) {" +
            "        var i = callbacks.indexOf(callback); if (i >= 0) { callbacks.splice(i, 1); }" +
            "      }" +
            "    }," +
            "    emitWebEvent: function(message) {" +
            "      var port = bridge();" +
            "      if (port) { port.postMessage(typeof message === 'string' ? message : JSON.stringify(message)); }" +
            "    }" +
            "  };" +
            "})();";

    // the chat page's phone layout is for a window up to this wide (its "max-width: 760px" rules); held sideways in a
    // world the phone is wider than that, and the page showed its desktop layout, squashed. So on a screen that wide
    // the page is laid out this wide and scaled to fit (its viewport tag, rewritten as it's parsed; the WebView takes
    // a page's viewport width only with wide viewports on, set for chat)
    private static final int CHAT_PHONE_WIDTH = 760;
    private static final String CHAT_VIEWPORT_SCRIPT =
            "(function() {" +
            "  if (window.top !== window || screen.width <= " + CHAT_PHONE_WIDTH + ") { return; }" +
            "  var content = 'width=" + CHAT_PHONE_WIDTH + ", viewport-fit=cover';" +
            "  function fix() {" +
            "    var meta = document.querySelector('meta[name=viewport]');" +
            "    if (!meta) { return false; }" +
            "    if (meta.content !== content) { meta.content = content; }" +
            "    return true;" +
            "  }" +
            "  if (fix()) { return; }" +
            "  var watch = new MutationObserver(function() { if (fix()) { watch.disconnect(); } });" +
            "  watch.observe(document, { childList: true, subtree: true });" +
            "  document.addEventListener('DOMContentLoaded', function() {" +
            "    watch.disconnect();" +
            "    if (!fix()) {" +
            "      var meta = document.createElement('meta');" +
            "      meta.name = 'viewport';" +
            "      meta.content = content;" +
            "      document.head.appendChild(meta);" +
            "    }" +
            "  });" +
            "})();";

    // the app's own pages (the radio's), served from its assets' html folder, and nothing else of them, at an https
    // address of their own
    private static final String APP_PAGES_ORIGIN = "https://" + WebViewAssetLoader.DEFAULT_DOMAIN;
    private static final String APP_PAGES_PATH = "/html/";

    // the one showing (UI thread)
    private static WebPanel sShowing;
    // the screen in front that isn't the world's (the Places screen, MainActivity), which the panels open over; the
    // world's activity when there is none (set on the UI thread, read on the engine's)
    private static volatile WeakReference<Activity> sHost;

    private final Activity mActivity;
    private final String mName;
    private final Uri mOrigin;               // where the page may go; the rest opens in the phone's browser
    private final boolean mChat;
    private final WebViewAssetLoader mAssets;
    private Dialog mDialog;
    private WebView mWebView;
    private JavaScriptReplyProxy mReply;     // the chat page's main frame, once it has said something
    private boolean mClosed;
    private boolean mReplaced;               // by another of the same name: the engine has the new one already

    /** From a screen of the app's own as it comes to the front: the panels open over it until it leaves. */
    public static void setHost(Activity activity) {
        sHost = new WeakReference<>(activity);
    }

    /** From that screen as it goes from the front. */
    public static void leaveHost(Activity activity) {
        WeakReference<Activity> host = sHost;
        if (host != null && host.get() == activity) {
            sHost = null;
        }
    }

    private static Activity host() {
        WeakReference<Activity> host = sHost;
        Activity activity = host != null ? host.get() : null;
        return activity != null && !activity.isFinishing() ? activity : QtNative.activity();
    }

    /** From the engine: chat's page at an address the engine made, with its bridge code, on the chat server's origin. */
    public static void openChat(String title, String url, String origin) {
        Activity activity = host();
        if (activity == null) {
            nativeClosed(CHAT);
            return;
        }
        activity.runOnUiThread(() -> {
            if (!WebViewFeature.isFeatureSupported(WebViewFeature.WEB_MESSAGE_LISTENER) ||
                    !WebViewFeature.isFeatureSupported(WebViewFeature.DOCUMENT_START_SCRIPT)) {
                Toast.makeText(activity, "Chat needs a newer Android System WebView: update it in the Play Store.",
                        Toast.LENGTH_LONG).show();
                nativeClosed(CHAT);
                return;
            }
            show(new WebPanel(activity, CHAT, Uri.parse(origin), true), title, url);
        });
    }

    /** From the engine: one of the app's own pages, by its path in the assets' html folder (the radio's). */
    public static void openAppPage(String name, String title, String path) {
        Activity activity = host();
        if (activity == null) {
            nativeClosed(name);
            return;
        }
        activity.runOnUiThread(() -> show(new WebPanel(activity, name, Uri.parse(APP_PAGES_ORIGIN), false), title,
                APP_PAGES_ORIGIN + APP_PAGES_PATH + path));
    }

    /** From the engine: closes the panel if it's the one showing. */
    public static void close(String name) {
        Activity activity = host();
        if (activity == null) {
            return;
        }
        activity.runOnUiThread(() -> {
            if (sShowing != null && sShowing.mName.equals(name)) {
                sShowing.mDialog.dismiss();
            }
        });
    }

    /** From the engine: a message for the chat page (EventBridge.scriptEventReceived's callbacks), as JSON text. */
    public static void sendToChat(String json) {
        Activity activity = host();
        if (activity == null) {
            return;
        }
        activity.runOnUiThread(() -> {
            WebPanel panel = sShowing;
            if (panel != null && panel.mChat && panel.mReply != null) {
                panel.mReply.postMessage(json);
            }
        });
    }

    private static void show(WebPanel panel, String title, String url) {
        if (sShowing != null) {
            sShowing.mReplaced = sShowing.mName.equals(panel.mName);
            sShowing.mDialog.dismiss();
        }
        sShowing = panel;
        panel.create(title);
        panel.mWebView.loadUrl(url);
    }

    private WebPanel(Activity activity, String name, Uri origin, boolean chat) {
        mActivity = activity;
        mName = name;
        mOrigin = origin;
        mChat = chat;
        if (chat) {
            mAssets = null;
        } else {
            WebViewAssetLoader.AssetsPathHandler assets = new WebViewAssetLoader.AssetsPathHandler(activity);
            mAssets = new WebViewAssetLoader.Builder()
                    .addPathHandler(APP_PAGES_PATH, path -> assets.handle("html/" + path))
                    .build();
        }
    }

    @SuppressLint("SetJavaScriptEnabled")
    private void create(String titleText) {
        Theme theme = ThemeManager.get(mActivity);
        float density = mActivity.getResources().getDisplayMetrics().density;

        // the page, and beside it on the right a narrow rail with the close box: the phone is held sideways, so a
        // header across the top took a sixth of the page's height (the title is the close box's description)
        LinearLayout root = new LinearLayout(mActivity);
        root.setOrientation(LinearLayout.HORIZONTAL);
        root.setBackgroundColor(theme.surface);

        LinearLayout rail = new LinearLayout(mActivity);
        rail.setOrientation(LinearLayout.VERTICAL);
        rail.setGravity(Gravity.CENTER_HORIZONTAL);
        int padding = (int) (4 * density);
        rail.setPadding(padding, padding * 2, padding, padding);
        ImageButton close = new ImageButton(mActivity);
        close.setImageResource(R.drawable.ic_close);
        close.setScaleType(ImageButton.ScaleType.FIT_CENTER);
        close.setBackground(null);
        close.setContentDescription(mActivity.getString(R.string.cancel) + " " + titleText);
        close.setOnClickListener(view -> mDialog.dismiss());
        int closeSize = (int) (36 * density);
        rail.addView(close, new LinearLayout.LayoutParams(closeSize, closeSize));

        mWebView = new WebView(mActivity);
        WebSettings settings = mWebView.getSettings();
        settings.setJavaScriptEnabled(true);
        settings.setDomStorageEnabled(true);
        settings.setAllowFileAccess(false);
        settings.setAllowContentAccess(false);
        // a new window (a link's target) is a navigation here, which the client sends to the browser
        settings.setSupportMultipleWindows(false);
        if (mChat) {
            // the page's own viewport width (CHAT_VIEWPORT_SCRIPT's), fitted to the view
            settings.setUseWideViewPort(true);
            settings.setLoadWithOverviewMode(true);
        } else {
            // the radio plays http:// streams too, from its https page
            settings.setMixedContentMode(WebSettings.MIXED_CONTENT_COMPATIBILITY_MODE);
        }
        mWebView.setBackgroundColor(theme.surface);
        mWebView.setWebViewClient(new PanelClient());
        mWebView.setWebChromeClient(new WebChromeClient());
        if (mChat) {
            Set<String> chatOrigin = Collections.singleton(originRule(mOrigin));
            WebViewCompat.addDocumentStartJavaScript(mWebView, CHAT_BRIDGE_SCRIPT, chatOrigin);
            WebViewCompat.addDocumentStartJavaScript(mWebView, CHAT_VIEWPORT_SCRIPT, chatOrigin);
            WebViewCompat.addWebMessageListener(mWebView, CHAT_BRIDGE_OBJECT, chatOrigin,
                    (view, message, sourceOrigin, isMainFrame, replyProxy) -> {
                        // the chat server's page itself: never a frame in it, a page it led to, or one gone
                        if (view != mWebView || mClosed || !isMainFrame || !sameOrigin(sourceOrigin, mOrigin)) {
                            Log.w(TAG, "Chat: a message from " + sourceOrigin + " ignored");
                            return;
                        }
                        String data = message.getData();
                        if (data == null) {
                            return;
                        }
                        mReply = replyProxy;
                        nativeChatMessage(data);
                    });
        }
        root.addView(mWebView, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.MATCH_PARENT, 1));
        root.addView(rail, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.MATCH_PARENT));

        mDialog = new Dialog(mActivity, android.R.style.Theme_DeviceDefault_NoActionBar);
        mDialog.setContentView(root);
        Window window = mDialog.getWindow();
        if (window != null) {
            window.setLayout(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
            window.setBackgroundDrawable(new ColorDrawable(theme.surface));
            // drawn behind the system bars (as every window is from Android 15), the page keeps clear of them and of
            // the keyboard by the room they take (below)
            WindowCompat.setDecorFitsSystemWindows(window, false);
            window.setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
            WindowInsetsControllerCompat bars = WindowCompat.getInsetsController(window, window.getDecorView());
            bars.setAppearanceLightStatusBars(theme.isLight());
            bars.setAppearanceLightNavigationBars(theme.isLight());
            // full screen as the world is: the system bars come back for a moment with a swipe from the edge
            bars.setSystemBarsBehavior(WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            bars.hide(WindowInsetsCompat.Type.systemBars());
        }
        ViewCompat.setOnApplyWindowInsetsListener(root, (view, insets) -> {
            Insets room = insets.getInsets(WindowInsetsCompat.Type.systemBars() |
                    WindowInsetsCompat.Type.displayCutout() | WindowInsetsCompat.Type.ime());
            view.setPadding(room.left, room.top, room.right, room.bottom);
            return WindowInsetsCompat.CONSUMED;
        });
        // back goes back in the page first, then closes it
        mDialog.setOnKeyListener((dialog, keyCode, event) -> {
            if (keyCode != KeyEvent.KEYCODE_BACK) {
                return false;
            }
            if (event.getAction() == KeyEvent.ACTION_UP) {
                if (mWebView != null && mWebView.canGoBack()) {
                    mWebView.goBack();
                } else {
                    dialog.dismiss();
                }
            }
            return true;
        });
        mDialog.setOnDismissListener(dialog -> closed());
        mDialog.show();
    }

    private void closed() {
        if (mClosed) {
            return;
        }
        mClosed = true;
        if (sShowing == this) {
            sShowing = null;
        }
        mReply = null;
        if (mWebView != null) {
            mWebView.stopLoading();
            mWebView.destroy();
            mWebView = null;
        }
        if (!mReplaced) {
            nativeClosed(mName);
        }
    }

    // "scheme://host[:port]", as addWebMessageListener's rules take an origin
    private static String originRule(Uri origin) {
        return origin.getScheme() + "://" + origin.getHost() + (origin.getPort() > 0 ? ":" + origin.getPort() : "");
    }

    private static boolean sameOrigin(Uri a, Uri b) {
        return a != null && b != null && a.getScheme() != null && a.getHost() != null &&
                a.getScheme().equalsIgnoreCase(b.getScheme()) && a.getHost().equalsIgnoreCase(b.getHost()) &&
                port(a) == port(b);
    }

    private static int port(Uri uri) {
        if (uri.getPort() > 0) {
            return uri.getPort();
        }
        return "http".equalsIgnoreCase(uri.getScheme()) ? 80 : 443;
    }

    private class PanelClient extends WebViewClient {
        @Override
        public boolean shouldOverrideUrlLoading(WebView view, WebResourceRequest request) {
            Uri url = request.getUrl();
            // frames load as they will (no bridge in them); the page itself stays on its origin
            if (!request.isForMainFrame() || sameOrigin(url, mOrigin)) {
                return false;
            }
            String scheme = url.getScheme() != null ? url.getScheme().toLowerCase() : "";
            if (scheme.equals("http") || scheme.equals("https") || scheme.equals("hifi")) {
                // a world's link comes back to the app, which joins it (its hifi:// intent): the world shows, not this
                if (scheme.equals("hifi")) {
                    mDialog.dismiss();
                }
                try {
                    mActivity.startActivity(new Intent(Intent.ACTION_VIEW, url).addCategory(Intent.CATEGORY_BROWSABLE));
                } catch (ActivityNotFoundException e) {
                    Log.w(TAG, "Nothing opens " + url);
                }
            }
            return true;
        }

        @Override
        public WebResourceResponse shouldInterceptRequest(WebView view, WebResourceRequest request) {
            return mAssets != null ? mAssets.shouldInterceptRequest(request.getUrl()) : null;
        }

        @Override
        public boolean onRenderProcessGone(WebView view, RenderProcessGoneDetail detail) {
            // the page's process went (out of memory): the panel closes rather than the app
            Log.w(TAG, mName + "'s page stopped");
            if (view == mWebView) {
                mWebView = null;
                ViewGroup parent = (ViewGroup) view.getParent();
                if (parent != null) {
                    parent.removeView(view);
                }
                view.destroy();
                mDialog.dismiss();
            }
            return true;
        }
    }
}
