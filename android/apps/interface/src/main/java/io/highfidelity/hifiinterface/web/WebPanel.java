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
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsControllerCompat;
import androidx.webkit.JavaScriptReplyProxy;
import androidx.webkit.WebViewCompat;
import androidx.webkit.WebViewFeature;

import org.qtproject.qt5.android.QtNative;

import java.util.Collections;
import java.util.Set;

import io.highfidelity.hifiinterface.R;
import io.highfidelity.hifiinterface.theme.Theme;
import io.highfidelity.hifiinterface.theme.ThemeManager;
import io.highfidelity.hifiinterface.theme.ThemeStyler;

/**
 * A web page full-screen over the world, with a close box: Darkride chat's web client. It's a
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

    // the one showing (UI thread)
    private static WebPanel sShowing;

    private final Activity mActivity;
    private final String mName;
    private final Uri mOrigin;               // where the page may go; the rest opens in the phone's browser
    private final boolean mChat;
    private Dialog mDialog;
    private WebView mWebView;
    private JavaScriptReplyProxy mReply;     // the chat page's main frame, once it has said something
    private boolean mClosed;
    private boolean mReplaced;               // by another of the same name: the engine has the new one already

    /** From the engine: chat's page at an address the engine made, with its bridge code, on the chat server's origin. */
    public static void openChat(String title, String url, String origin) {
        Activity activity = QtNative.activity();
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

    /** From the engine: closes the panel if it's the one showing. */
    public static void close(String name) {
        Activity activity = QtNative.activity();
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
        Activity activity = QtNative.activity();
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
    }

    @SuppressLint("SetJavaScriptEnabled")
    private void create(String titleText) {
        Theme theme = ThemeManager.get(mActivity);
        float density = mActivity.getResources().getDisplayMetrics().density;

        LinearLayout root = new LinearLayout(mActivity);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(theme.surface);

        // as the Settings panel's header (InterfaceActivity)
        LinearLayout header = new LinearLayout(mActivity);
        header.setOrientation(LinearLayout.HORIZONTAL);
        header.setGravity(Gravity.CENTER_VERTICAL);
        int padding = (int) (16 * density);
        header.setPadding(padding, padding / 2, padding / 2, padding / 2);
        TextView title = new TextView(mActivity);
        title.setText(titleText);
        title.setTextColor(theme.text);
        title.setTextSize(20);
        ThemeStyler.styleDisplay(title, theme);
        header.addView(title, new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1));
        ImageButton close = new ImageButton(mActivity);
        close.setImageResource(R.drawable.ic_close);
        close.setScaleType(ImageButton.ScaleType.FIT_CENTER);
        close.setBackground(null);
        close.setContentDescription(mActivity.getString(R.string.cancel));
        close.setOnClickListener(view -> mDialog.dismiss());
        int closeSize = (int) (40 * density);
        header.addView(close, new LinearLayout.LayoutParams(closeSize, closeSize));
        root.addView(header);

        mWebView = new WebView(mActivity);
        WebSettings settings = mWebView.getSettings();
        settings.setJavaScriptEnabled(true);
        settings.setDomStorageEnabled(true);
        settings.setAllowFileAccess(false);
        settings.setAllowContentAccess(false);
        // a new window (a link's target) is a navigation here, which the client sends to the browser
        settings.setSupportMultipleWindows(false);
        mWebView.setBackgroundColor(theme.surface);
        mWebView.setWebViewClient(new PanelClient());
        mWebView.setWebChromeClient(new WebChromeClient());
        if (mChat) {
            Set<String> chatOrigin = Collections.singleton(originRule(mOrigin));
            WebViewCompat.addDocumentStartJavaScript(mWebView, CHAT_BRIDGE_SCRIPT, chatOrigin);
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
        root.addView(mWebView, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1));

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
