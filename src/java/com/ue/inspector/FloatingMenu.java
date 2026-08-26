package com.ue.inspector;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

public class FloatingMenu {

    private static Button floatingBtn = null;
    private static LinearLayout menuPanel = null;

    // Native JNI Methods
    public static native String nativeGetUEInfo();
    public static native String nativeGetObjectsList(String query);
    public static native String nativeDumpSDK();

    public static void show(final Activity activity) {
        if (activity == null) return;

        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    final ViewGroup decorView = (ViewGroup) activity.getWindow().getDecorView();
                    if (decorView == null) return;

                    // Clean up if already exists
                    if (floatingBtn != null && floatingBtn.getParent() != null) {
                        ((ViewGroup) floatingBtn.getParent()).removeView(floatingBtn);
                    }
                    if (menuPanel != null && menuPanel.getParent() != null) {
                        ((ViewGroup) menuPanel.getParent()).removeView(menuPanel);
                    }

                    // 1. Create Main Inspector Overlay Panel (Hidden initially, directly in DecorView)
                    menuPanel = new LinearLayout(activity);
                    menuPanel.setOrientation(LinearLayout.VERTICAL);
                    menuPanel.setPadding(35, 35, 35, 35);
                    menuPanel.setVisibility(View.GONE);

                    GradientDrawable panelBg = new GradientDrawable();
                    panelBg.setColor(Color.parseColor("#F014141E")); // Sleek dark translucent
                    panelBg.setCornerRadius(28.0f);
                    panelBg.setStroke(3, Color.parseColor("#FF2A85FF")); // Futuristic cyan-blue stroke
                    menuPanel.setBackground(panelBg);

                    // Panel Layout Params: Centered overlay over game screen
                    int screenWidth = activity.getResources().getDisplayMetrics().widthPixels;
                    int screenHeight = activity.getResources().getDisplayMetrics().heightPixels;
                    int panelWidth = (int) (Math.min(screenWidth, screenHeight) * 0.92f);

                    FrameLayout.LayoutParams panelParams = new FrameLayout.LayoutParams(panelWidth, FrameLayout.LayoutParams.WRAP_CONTENT);
                    panelParams.gravity = Gravity.CENTER;
                    panelParams.setMargins(20, 20, 20, 20);

                    // Header Title
                    TextView title = new TextView(activity);
                    title.setText("⚡ UE Mobile Inspector v1.0 (Delta Force)");
                    title.setTextSize(17);
                    title.setTextColor(Color.parseColor("#FF2A85FF"));
                    title.setGravity(Gravity.CENTER);
                    menuPanel.addView(title);

                    // Status Text
                    final TextView statusText = new TextView(activity);
                    statusText.setTextColor(Color.parseColor("#4CAF50"));
                    statusText.setTextSize(12);
                    statusText.setPadding(0, 12, 0, 12);
                    try {
                        statusText.setText(nativeGetUEInfo());
                    } catch (Throwable ignored) {
                        statusText.setText("Engine: Reflection Engine Connected (libUE4.so)");
                    }
                    menuPanel.addView(statusText);

                    // Search Box
                    final EditText searchBox = new EditText(activity);
                    searchBox.setHint("Search UObject / Actor / Class...");
                    searchBox.setHintTextColor(Color.LTGRAY);
                    searchBox.setTextColor(Color.WHITE);
                    searchBox.setTextSize(13);
                    searchBox.setBackgroundColor(Color.parseColor("#25FFFFFF"));
                    searchBox.setPadding(20, 16, 20, 16);
                    menuPanel.addView(searchBox);

                    // Action Buttons Row
                    LinearLayout btnRow = new LinearLayout(activity);
                    btnRow.setOrientation(LinearLayout.HORIZONTAL);
                    btnRow.setPadding(0, 16, 0, 16);

                    Button searchBtn = createStyledButton(activity, "🔍 搜索", "#2196F3");
                    Button dumpBtn = createStyledButton(activity, "💾 Dump SDK", "#9C27B0");
                    Button closeBtn = createStyledButton(activity, "✖ 隐藏", "#F44336");

                    btnRow.addView(searchBtn);
                    btnRow.addView(dumpBtn);
                    btnRow.addView(closeBtn);
                    menuPanel.addView(btnRow);

                    // Scrollable Output View
                    ScrollView scrollView = new ScrollView(activity);
                    LinearLayout.LayoutParams scrollParams = new LinearLayout.LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT, (int) (screenHeight * 0.45f));
                    scrollView.setLayoutParams(scrollParams);

                    final TextView outputText = new TextView(activity);
                    outputText.setTextColor(Color.parseColor("#E0E0E0"));
                    outputText.setTextSize(11);
                    outputText.setTypeface(android.graphics.Typeface.MONOSPACE);
                    outputText.setText("Click [🔍 搜索] to scan running UObject reflection tree...");

                    scrollView.addView(outputText);
                    menuPanel.addView(scrollView);

                    // Event Listeners for Panel
                    searchBtn.setOnClickListener(new View.OnClickListener() {
                        @Override
                        public void onClick(View v) {
                            try {
                                statusText.setText(nativeGetUEInfo());
                                String query = searchBox.getText().toString();
                                outputText.setText(nativeGetObjectsList(query));
                            } catch (Throwable t) {
                                outputText.setText("Search failed: " + t.getMessage());
                            }
                        }
                    });

                    dumpBtn.setOnClickListener(new View.OnClickListener() {
                        @Override
                        public void onClick(View v) {
                            try {
                                String res = nativeDumpSDK();
                                Toast.makeText(activity, res, Toast.LENGTH_LONG).show();
                                outputText.setText(res);
                            } catch (Throwable t) {
                                Toast.makeText(activity, "Dump error: " + t.getMessage(), Toast.LENGTH_LONG).show();
                            }
                        }
                    });

                    closeBtn.setOnClickListener(new View.OnClickListener() {
                        @Override
                        public void onClick(View v) {
                            menuPanel.setVisibility(View.GONE);
                            if (floatingBtn != null) floatingBtn.setVisibility(View.VISIBLE);
                        }
                    });

                    // 2. Create Floating Toggle Button
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
                                    if (Math.abs(dx) > 10 || Math.abs(dy) > 10) {
                                        isDragging = true;
                                    }
                                    if (isDragging) {
                                        btnParams.leftMargin = (int) (initialX + dx);
                                        btnParams.topMargin = (int) (initialY + dy);
                                        v.setLayoutParams(btnParams);
                                    }
                                    return true;

                                case MotionEvent.ACTION_UP:
                                    if (!isDragging) {
                                        // Toggle Menu View smoothly without Dialog
                                        if (menuPanel != null) {
                                            menuPanel.setVisibility(View.VISIBLE);
                                            try {
                                                statusText.setText(nativeGetUEInfo());
                                            } catch (Throwable ignored) {}
                                        }
                                    }
                                    return true;
                            }
                            return false;
                        }
                    });

                    // Add both View Overlay and Floating Button directly to DecorView
                    decorView.addView(menuPanel, panelParams);
                    decorView.addView(floatingBtn, btnParams);

                    Toast.makeText(activity, "[SV] UE Mobile Inspector Loaded! Click [UE] icon.", Toast.LENGTH_LONG).show();

                } catch (Throwable t) {
                    t.printStackTrace();
                }
            }
        });
    }

    private static Button createStyledButton(Context context, String text, String colorHex) {
        Button btn = new Button(context);
        btn.setText(text);
        btn.setTextColor(Color.WHITE);
        btn.setTextSize(13);

        GradientDrawable bg = new GradientDrawable();
        bg.setColor(Color.parseColor(colorHex));
        bg.setCornerRadius(15.0f);
        btn.setBackground(bg);

        LinearLayout.LayoutParams p = new LinearLayout.LayoutParams(0, 110, 1.0f);
        p.setMargins(10, 0, 10, 0);
        btn.setLayoutParams(p);
        return btn;
    }
}
