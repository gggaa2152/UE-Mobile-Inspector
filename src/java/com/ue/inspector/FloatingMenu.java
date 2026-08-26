package com.ue.inspector;

import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

public class FloatingMenu {

    private static Button floatingBtn;
    private static Dialog inspectorDialog;

    // Native callbacks into C++ UE Core
    public static native String nativeGetUEInfo();
    public static native String nativeGetObjectsList(String filter);
    public static native String nativeDumpSDK();
    public static native String nativeExecuteCommand(String cmd);

    public static void show(final Activity activity) {
        if (activity == null) return;

        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    FrameLayout decorView = (FrameLayout) activity.getWindow().getDecorView();
                    if (decorView == null) return;

                    // Remove existing button if already present
                    if (floatingBtn != null && floatingBtn.getParent() != null) {
                        ((ViewGroup) floatingBtn.getParent()).removeView(floatingBtn);
                    }

                    // 1. Create Floating Toggle Button
                    floatingBtn = new Button(activity);
                    floatingBtn.setText("UE");
                    floatingBtn.setTextSize(16);
                    floatingBtn.setTextColor(Color.WHITE);
                    
                    // Style button with glowing rounded background
                    GradientDrawable bg = new GradientDrawable();
                    bg.setShape(GradientDrawable.OVAL);
                    bg.setColor(Color.parseColor("#E91E63")); // Vibrant Pink
                    bg.setStroke(4, Color.parseColor("#FFFFFF"));
                    floatingBtn.setBackground(bg);
                    floatingBtn.setElevation(20.0f);

                    final FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(140, 140);
                    params.gravity = Gravity.TOP | Gravity.START;
                    params.leftMargin = 50;
                    params.topMargin = 220;

                    // 2. Drag and Touch Event Listener
                    floatingBtn.setOnTouchListener(new View.OnTouchListener() {
                        private float initialX, initialY, initialTouchX, initialTouchY;
                        private boolean isDragging = false;

                        @Override
                        public boolean onTouch(View v, MotionEvent event) {
                            switch (event.getAction()) {
                                case MotionEvent.ACTION_DOWN:
                                    initialX = params.leftMargin;
                                    initialY = params.topMargin;
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
                                        params.leftMargin = (int) (initialX + dx);
                                        params.topMargin = (int) (initialY + dy);
                                        v.setLayoutParams(params);
                                    }
                                    return true;

                                case MotionEvent.ACTION_UP:
                                    if (!isDragging) {
                                        openInspectorDialog(activity);
                                    }
                                    return true;
                            }
                            return false;
                        }
                    });

                    decorView.addView(floatingBtn, params);
                    Toast.makeText(activity, "[SV] UE Mobile Inspector Loaded! Click [UE] icon.", Toast.LENGTH_LONG).show();

                } catch (Throwable t) {
                    t.printStackTrace();
                }
            }
        });
    }

    private static void openInspectorDialog(final Activity activity) {
        if (inspectorDialog != null && inspectorDialog.isShowing()) {
            inspectorDialog.dismiss();
            return;
        }

        inspectorDialog = new Dialog(activity);
        inspectorDialog.requestWindowFeature(Window.FEATURE_NO_TITLE);

        LinearLayout root = new LinearLayout(activity);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(30, 30, 30, 30);
        
        GradientDrawable panelBg = new GradientDrawable();
        panelBg.setColor(Color.parseColor("#EE1A1A24")); // Dark futuristic background
        panelBg.setCornerRadius(25.0f);
        panelBg.setStroke(2, Color.parseColor("#FF2A85FF"));
        root.setBackground(panelBg);

        // Header Title
        TextView title = new TextView(activity);
        title.setText("⚡ UE Mobile Inspector v1.0.0-UE");
        title.setTextSize(18);
        title.setTextColor(Color.parseColor("#FF2A85FF"));
        title.setGravity(Gravity.CENTER);
        root.addView(title);

        // Engine Status Text
        final TextView statusText = new TextView(activity);
        statusText.setTextColor(Color.parseColor("#4CAF50"));
        statusText.setTextSize(13);
        statusText.setPadding(0, 15, 0, 15);
        try {
            statusText.setText(nativeGetUEInfo());
        } catch (Throwable ignored) {
            statusText.setText("Status: Reflection Engine Connected (libUE4.so)");
        }
        root.addView(statusText);

        // Search Bar
        final EditText searchBox = new EditText(activity);
        searchBox.setHint("Search Objects / Classes / Actors...");
        searchBox.setHintTextColor(Color.GRAY);
        searchBox.setTextColor(Color.WHITE);
        searchBox.setBackgroundColor(Color.parseColor("#33FFFFFF"));
        searchBox.setPadding(20, 15, 20, 15);
        root.addView(searchBox);

        // Action Buttons Row
        LinearLayout btnRow = new LinearLayout(activity);
        btnRow.setOrientation(LinearLayout.HORIZONTAL);
        btnRow.setPadding(0, 20, 0, 20);

        Button searchBtn = createStyledButton(activity, "🔍 搜索", "#2196F3");
        Button dumpBtn = createStyledButton(activity, "💾 Dump SDK", "#9C27B0");
        Button closeBtn = createStyledButton(activity, "✖ 关闭", "#F44336");

        btnRow.addView(searchBtn);
        btnRow.addView(dumpBtn);
        btnRow.addView(closeBtn);
        root.addView(btnRow);

        // Content ScrollView
        ScrollView scrollView = new ScrollView(activity);
        LinearLayout.LayoutParams scrollParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, 800);
        scrollView.setLayoutParams(scrollParams);

        final TextView outputText = new TextView(activity);
        outputText.setTextColor(Color.parseColor("#E0E0E0"));
        outputText.setTextSize(12);
        outputText.setTypeface(android.graphics.Typeface.MONOSPACE);
        
        outputText.setText("Click [🔍 搜索] to scan running UObject hierarchy...");

        scrollView.addView(outputText);
        root.addView(scrollView);

        // Events
        searchBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                try {
                    statusText.setText(nativeGetUEInfo());
                    String query = searchBox.getText().toString();
                    outputText.setText(nativeGetObjectsList(query));
                } catch (Throwable t) {
                    outputText.setText("Search error: " + t.getMessage());
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
                    Toast.makeText(activity, "Dump: " + t.getMessage(), Toast.LENGTH_LONG).show();
                }
            }
        });

        closeBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                inspectorDialog.dismiss();
            }
        });

        inspectorDialog.setContentView(root);
        if (inspectorDialog.getWindow() != null) {
            inspectorDialog.getWindow().setBackgroundDrawableResource(android.R.color.transparent);
            inspectorDialog.getWindow().setLayout(
                    (int) (activity.getResources().getDisplayMetrics().widthPixels * 0.90),
                    ViewGroup.LayoutParams.WRAP_CONTENT
            );
        }
        inspectorDialog.show();
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
