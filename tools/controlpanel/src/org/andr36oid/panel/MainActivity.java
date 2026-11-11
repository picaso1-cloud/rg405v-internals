package org.andr36oid.panel;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.graphics.Typeface;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.LinearLayout;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.Switch;
import android.widget.TextView;
import android.widget.Toast;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * andr36oid Panel — small control surface for the mods we run on the RG405V:
 * mouse mode, USB function, GammaOS performance profile and the HID gamepad.
 *
 * Everything is executed through Magisk's `su`, so the app itself needs no
 * special permissions.
 */
public class MainActivity extends Activity {

    private static final String PREFS = "panel";
    private static final String MOUSEMODE = "/data/local/tmp/mousemode";
    private static final String CONF = "/data/local/tmp/mousemode.conf";
    private static final String HIDSCRIPT = "/data/local/tmp/hid-gadget.sh";

    private final ExecutorService pool = Executors.newSingleThreadExecutor();
    private final Handler ui = new Handler(Looper.getMainLooper());

    private SharedPreferences prefs;

    private TextView statusLine;
    private Switch mouseSwitch;
    private SeekBar speedBar;
    private TextView speedLabel;
    private SeekBar deadzoneBar;
    private TextView deadzoneLabel;
    private CheckBox invertCheck;
    private CheckBox abLeftCheck;

    private int dp(float v) {
        return Math.round(v * getResources().getDisplayMetrics().density);
    }

    private TextView heading(String text) {
        TextView t = new TextView(this);
        t.setText(text);
        t.setTextSize(18);
        t.setTypeface(Typeface.DEFAULT_BOLD);
        t.setPadding(0, dp(18), 0, dp(6));
        return t;
    }

    private TextView note(String text) {
        TextView t = new TextView(this);
        t.setText(text);
        t.setTextSize(12);
        t.setTextColor(Color.GRAY);
        t.setPadding(0, 0, 0, dp(4));
        return t;
    }

    private Button action(String text, Runnable r) {
        Button b = new Button(this);
        b.setText(text);
        b.setAllCaps(false);
        b.setOnClickListener(v -> r.run());
        return b;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        prefs = getSharedPreferences(PREFS, MODE_PRIVATE);

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(dp(20), dp(16), dp(20), dp(28));

        TextView title = new TextView(this);
        title.setText("andr36oid Panel");
        title.setTextSize(24);
        title.setTypeface(Typeface.DEFAULT_BOLD);
        root.addView(title);

        statusLine = note("checking...");
        root.addView(statusLine);

        // ---------------- Mouse ----------------
        root.addView(heading("Mouse mode"));
        root.addView(note("Toggle on the device with L1 + R1 + Select. "
                + "While active the gamepad is captured and drives the pointer."));

        mouseSwitch = new Switch(this);
        mouseSwitch.setText("Enable mouse mode");
        mouseSwitch.setOnCheckedChangeListener((v, checked) -> {
            if (checked) startMouse();
            else stopMouse();
        });
        root.addView(mouseSwitch);

        speedLabel = note("Pointer speed: " + prefs.getInt("speed", 700));
        root.addView(speedLabel);
        speedBar = new SeekBar(this);
        speedBar.setMax(2800);
        speedBar.setProgress(prefs.getInt("speed", 700) - 200);
        speedBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            public void onProgressChanged(SeekBar s, int p, boolean fromUser) {
                speedLabel.setText("Pointer speed: " + (p + 200));
            }
            public void onStartTrackingTouch(SeekBar s) { }
            public void onStopTrackingTouch(SeekBar s) { applyAndRestart(); }
        });
        root.addView(speedBar);

        deadzoneLabel = note("Deadzone: " + prefs.getInt("deadzone", 300));
        root.addView(deadzoneLabel);
        deadzoneBar = new SeekBar(this);
        deadzoneBar.setMax(900);
        deadzoneBar.setProgress(prefs.getInt("deadzone", 300));
        deadzoneBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            public void onProgressChanged(SeekBar s, int p, boolean fromUser) {
                deadzoneLabel.setText("Deadzone: " + p);
            }
            public void onStartTrackingTouch(SeekBar s) { }
            public void onStopTrackingTouch(SeekBar s) { applyAndRestart(); }
        });
        root.addView(deadzoneBar);

        invertCheck = new CheckBox(this);
        invertCheck.setText("Invert pointer Y");
        invertCheck.setChecked(prefs.getBoolean("invert", false));
        invertCheck.setOnCheckedChangeListener((v, c) -> applyAndRestart());
        root.addView(invertCheck);

        abLeftCheck = new CheckBox(this);
        abLeftCheck.setText("A button = left click (Nintendo layout)");
        abLeftCheck.setChecked(prefs.getBoolean("ab_left", true));
        abLeftCheck.setOnCheckedChangeListener((v, c) -> applyAndRestart());
        root.addView(abLeftCheck);

        LinearLayout mouseButtons = new LinearLayout(this);
        mouseButtons.setOrientation(LinearLayout.HORIZONTAL);
        mouseButtons.addView(action("Apply & restart", this::applyAndRestart),
                new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1));
        root.addView(mouseButtons);

        // ---------------- USB ----------------
        root.addView(heading("USB mode"));
        root.addView(note("ADB stays available over Wi-Fi on port 5555."));

        RadioGroup usbGroup = new RadioGroup(this);
        usbGroup.setOrientation(RadioGroup.VERTICAL);
        String[] usbModes = {"mtp", "ptp", "rndis", "midi", "adb", "charging"};
        String[] usbLabels = {"MTP — file transfer", "PTP — camera", "RNDIS — USB tethering",
                "MIDI", "ADB only", "Charging only"};
        for (int i = 0; i < usbModes.length; i++) {
            RadioButton rb = new RadioButton(this);
            rb.setText(usbLabels[i]);
            rb.setId(i + 100);
            rb.setTag(usbModes[i]);
            usbGroup.addView(rb);
        }
        root.addView(usbGroup);
        root.addView(action("Apply USB mode", () -> {
            int id = usbGroup.getCheckedRadioButtonId();
            if (id < 0) { toast("Pick a USB mode first"); return; }
            String mode = (String) ((RadioButton) usbGroup.findViewById(id)).getTag();
            String cmd = "charging".equals(mode)
                    ? "svc usb setFunctions"
                    : "svc usb setFunctions " + mode;
            runRoot(cmd, "USB -> " + mode);
        }));

        // ---------------- Performance ----------------
        root.addView(heading("Performance profile"));
        root.addView(note("Stock = schedutil + ondemand GPU (recommended). "
                + "Max pins 2.0 GHz / 850 MHz. Powersave caps clocks."));

        RadioGroup perfGroup = new RadioGroup(this);
        perfGroup.setOrientation(RadioGroup.VERTICAL);
        String[] perfModes = {"stock", "max", "powersave"};
        String[] perfLabels = {"Stock — balanced", "Max — gaming", "Powersave — endurance"};
        for (int i = 0; i < perfModes.length; i++) {
            RadioButton rb = new RadioButton(this);
            rb.setText(perfLabels[i]);
            rb.setId(i + 200);
            rb.setTag(perfModes[i]);
            perfGroup.addView(rb);
        }
        root.addView(perfGroup);
        root.addView(action("Apply profile", () -> {
            int id = perfGroup.getCheckedRadioButtonId();
            if (id < 0) { toast("Pick a profile first"); return; }
            String mode = (String) ((RadioButton) perfGroup.findViewById(id)).getTag();
            runRoot("setprop persist.gammaos.performance_mode " + mode, "profile -> " + mode);
        }));

        // ---------------- HID ----------------
        root.addView(heading("USB gamepad (HID)"));
        root.addView(note("Presents the handheld as a USB gamepad to a PC. "
                + "USB ADB and MTP stop working while active — use Wi-Fi ADB or "
                + "'Restore USB'."));

        LinearLayout hidRow = new LinearLayout(this);
        hidRow.setOrientation(LinearLayout.HORIZONTAL);
        hidRow.addView(action("Enable HID", this::confirmHid),
                new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1));
        hidRow.addView(action("Restore USB", () ->
                        runRoot("sh " + HIDSCRIPT + " stop", "USB restored")),
                new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1));
        root.addView(hidRow);

        root.addView(action("Refresh status", this::refreshStatus));

        ScrollView scroll = new ScrollView(this);
        scroll.addView(root);
        setContentView(scroll);

        refreshStatus();
    }

    // ------------------------------------------------------------------

    private void confirmHid() {
        new AlertDialog.Builder(this)
                .setTitle("Enable USB gamepad?")
                .setMessage("USB ADB/MTP will stop until you tap 'Restore USB', "
                        + "unplug/replug, or reboot.\n\n"
                        + "Make sure Wi-Fi ADB is working if you need to control the device.")
                .setPositiveButton("Enable", (d, w) ->
                        runRoot("sh " + HIDSCRIPT + " start", "HID enabled"))
                .setNegativeButton("Cancel", null)
                .show();
    }

    private void startMouse() {
        writeConf();
        runRoot("pkill -x mousemode; sleep 1; nohup " + MOUSEMODE
                + " >> /data/local/tmp/mousemode.log 2>&1 &", "mouse mode on");
    }

    private void stopMouse() {
        runRoot("pkill -x mousemode", "mouse mode off");
    }

    private void applyAndRestart() {
        int speed = speedBar.getProgress() + 200;
        int dz = deadzoneBar.getProgress();
        boolean inv = invertCheck.isChecked();
        prefs.edit().putInt("speed", speed).putInt("deadzone", dz)
                .putBoolean("invert", inv)
                .putBoolean("ab_left", abLeftCheck.isChecked()).apply();
        writeConf();
        if (mouseSwitch.isChecked()) startMouse();
        toast("Mouse settings applied");
    }

    private void writeConf() {
        int speed = speedBar.getProgress() + 200;
        int dz = deadzoneBar.getProgress();
        int inv = invertCheck.isChecked() ? 1 : 0;
        int ab = abLeftCheck.isChecked() ? 1 : 0;
        runRoot("printf 'speed=%d\\ndeadzone=%d\\ninvert_y=%d\\nab_left=%d\\n' "
                + speed + " " + dz + " " + inv + " " + ab
                + " > " + CONF + "; chmod 666 " + CONF, null);
    }

    private void refreshStatus() {
        pool.execute(() -> {
            String mouse = runRootSync("pidof mousemode");
            String perf = runRootSync("getprop persist.gammaos.performance_mode");
            String usb = runRootSync("getprop sys.usb.config");
            String hid = runRootSync("cat /sys/class/udc/musb-hdrc.1.auto/function 2>/dev/null");
            boolean mouseOn = !mouse.trim().isEmpty();
            String text = "mouse: " + (mouseOn ? "ON" : "off")
                    + "   profile: " + perf.trim()
                    + "   usb: " + usb.trim()
                    + "   HID: " + (hid.contains("g2") ? "ACTIVE" : "off");
            ui.post(() -> {
                statusLine.setText(text);
                mouseSwitch.setChecked(mouseOn);
            });
        });
    }

    // ------------------------------------------------------------------

    private void runRoot(String cmd, String okMessage) {
        pool.execute(() -> {
            String out = runRootSync(cmd);
            ui.post(() -> {
                if (okMessage != null) toast(okMessage);
                refreshStatus();
                if (out != null && !out.isEmpty() && !"0".equals(out.trim())) {
                    // keep output visible for debugging but do not spam toasts
                    statusLine.setText(statusLine.getText() + "\n" + out.trim());
                }
            });
        });
    }

    private String runRootSync(String cmd) {
        StringBuilder sb = new StringBuilder();
        try {
            Process p = Runtime.getRuntime().exec(new String[]{"su", "-c", cmd});
            BufferedReader r = new BufferedReader(new InputStreamReader(p.getInputStream()));
            String line;
            while ((line = r.readLine()) != null) sb.append(line).append('\n');
            BufferedReader e = new BufferedReader(new InputStreamReader(p.getErrorStream()));
            while ((line = e.readLine()) != null) sb.append(line).append('\n');
            p.waitFor();
        } catch (Exception ex) {
            return "ERR " + ex.getMessage();
        }
        return sb.toString();
    }

    private void toast(String s) {
        Toast.makeText(this, s, Toast.LENGTH_SHORT).show();
    }
}
