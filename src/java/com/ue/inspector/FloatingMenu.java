package com.ue.inspector;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.JavascriptInterface;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.Toast;

public class FloatingMenu {

    private static Button floatingBtn = null;
    private static FrameLayout webContainer = null;
    private static WebView webView = null;

    // Native JNI Methods
    public static native String nativeGetUEInfo();
    public static native String nativeGetClasses(String query);
    public static native String nativeGetInstances(String className);
    public static native String nativeInspectObject(String objAddrHex);
    public static native String nativeModifyField(String objAddrHex, String offsetHex, String type, String newVal);
    public static native String nativeDumpSDK();
    public static native String nativeExecuteConsole(String cmd);
    public static native String nativeGetHtmlSource();

    public static class NativeBridge {
        private final Activity activity;
        public NativeBridge(Activity act) { this.activity = act; }

        @JavascriptInterface
        public String getEngineInfo() {
            try { return nativeGetUEInfo(); } catch (Throwable t) { return "{}"; }
        }

        @JavascriptInterface
        public String getClassesList(String query) {
            try { return nativeGetClasses(query); } catch (Throwable t) { return "[]"; }
        }

        @JavascriptInterface
        public String getInstancesList(String className) {
            try { return nativeGetInstances(className); } catch (Throwable t) { return "[]"; }
        }

        @JavascriptInterface
        public String inspectObject(String objAddrHex) {
            try { return nativeInspectObject(objAddrHex); } catch (Throwable t) { return "{}"; }
        }

        @JavascriptInterface
        public String modifyFieldValue(String objAddrHex, String offsetHex, String type, String newVal) {
            try { return nativeModifyField(objAddrHex, offsetHex, type, newVal); } catch (Throwable t) { return "error"; }
        }

        @JavascriptInterface
        public String dumpSDK() {
            try { return nativeDumpSDK(); } catch (Throwable t) { return "error"; }
        }

        @JavascriptInterface
        public String executeConsoleCmd(String cmd) {
            try { return nativeExecuteConsole(cmd); } catch (Throwable t) { return "error"; }
        }

        @JavascriptInterface
        public void minimizeWindow() {
            if (activity != null) {
                activity.runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        if (webContainer != null) webContainer.setVisibility(View.GONE);
                        if (floatingBtn != null) floatingBtn.setVisibility(View.VISIBLE);
                    }
                });
            }
        }
    }

    public static void show(final Activity activity) {
        if (activity == null) return;

        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    final ViewGroup decorView = (ViewGroup) activity.getWindow().getDecorView();
                    if (decorView == null) return;

                    if (floatingBtn != null && floatingBtn.getParent() != null) {
                        ((ViewGroup) floatingBtn.getParent()).removeView(floatingBtn);
                    }
                    if (webContainer != null && webContainer.getParent() != null) {
                        ((ViewGroup) webContainer.getParent()).removeView(webContainer);
                    }

                    // 1. Create Web View Container directly inside DecorView
                    webContainer = new FrameLayout(activity);
                    webContainer.setVisibility(View.GONE);

                    FrameLayout.LayoutParams containerParams = new FrameLayout.LayoutParams(
                            FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT);
                    webContainer.setLayoutParams(containerParams);

                    // 2. Initialize WebView
                    webView = new WebView(activity);
                    webView.setBackgroundColor(Color.TRANSPARENT);
                    webView.setLayerType(View.LAYER_TYPE_HARDWARE, null);

                    WebSettings settings = webView.getSettings();
                    settings.setJavaScriptEnabled(true);
                    settings.setDomStorageEnabled(true);
                    settings.setAllowFileAccess(true);
                    settings.setAllowContentAccess(true);

                    webView.setWebViewClient(new WebViewClient());
                    webView.setWebChromeClient(new WebChromeClient());
                    webView.addJavascriptInterface(new NativeBridge(activity), "nativeAPI");

                    // Load full cyberpunk inspector HTML
                    String htmlContent = nativeGetHtmlSource();
                    webView.loadDataWithBaseURL("https://ue-inspector.local/", htmlContent, "text/html", "UTF-8", null);

                    webContainer.addView(webView, new FrameLayout.LayoutParams(
                            FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));

                    // 3. Create Floating Ball Toggle Button
                    floatingBtn = new Button(activity);
                    floatingBtn.setText("UE");
                    floatingBtn.setTextSize(16);
                    floatingBtn.setTextColor(Color.WHITE);

                    GradientDrawable btnBg = new GradientDrawable();
                    btnBg.setShape(GradientDrawable.OVAL);
                    btnBg.setColor(Color.parseColor("#E91E63"));
                    btnBg.setStroke(4, Color.parseColor("#FFFFFF"));
                    floatingBtn.setBackground(btnBg);
                    floatingBtn.setElevation(25.0f);

                    final FrameLayout.LayoutParams btnParams = new FrameLayout.LayoutParams(140, 140);
                    btnParams.gravity = Gravity.TOP | Gravity.START;
                    btnParams.leftMargin = 50;
                    btnParams.topMargin = 220;

                    floatingBtn.setOnTouchListener(new View.OnTouchListener() {
                        private float initialX, initialY, initialTouchX, initialTouchY;
                        private boolean isDragging = false;

                        @Override
                        public boolean onTouch(View v, MotionEvent event) {
                            switch (event.getAction()) {
                                case MotionEvent.ACTION_DOWN:
                                    initialX = btnParams.leftMargin;
                                    initialY = btnParams.topMargin;
                                    initialTouchX = event.getRawX();
                                    initialTouchY = event.getRawY();
                                    isDragging = false;
                                    return true;

                                case MotionEvent.ACTION_MOVE:
                                    float dx = event.getRawX() - initialTouchX;
                                    float dy = event.getRawY() - initialTouchY;
                                    if (Math.abs(dx) > 10 || Math.abs(dy) > 10) isDragging = true;
                                    if (isDragging) {
                                        btnParams.leftMargin = (int) (initialX + dx);
                                        btnParams.topMargin = (int) (initialY + dy);
                                        v.setLayoutParams(btnParams);
                                    }
                                    return true;

                                case MotionEvent.ACTION_UP:
                                    if (!isDragging) {
                                        if (webContainer != null) {
                                            webContainer.setVisibility(webContainer.getVisibility() == View.VISIBLE ? View.GONE : View.VISIBLE);
                                        }
                                    }
                                    return true;
                            }
                            return false;
                        }
                    });

                    // Add Views to DecorView
                    decorView.addView(webContainer);
                    decorView.addView(floatingBtn, btnParams);

                    Toast.makeText(activity, "[SV] UE Mobile Inspector Loaded! Click [UE] button.", Toast.LENGTH_LONG).show();

                } catch (Throwable t) {
                    t.printStackTrace();
                }
            }
        });
    }
}
