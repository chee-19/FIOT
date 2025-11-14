package com.sp.sensorvisualizer;

import androidx.appcompat.app.AppCompatActivity;
import android.annotation.SuppressLint;
import android.os.Bundle;
import android.os.Handler;
import android.os.StrictMode;
import android.view.Window;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;

import com.journeyapps.barcodescanner.ScanContract;
import com.journeyapps.barcodescanner.ScanOptions;
import androidx.activity.result.ActivityResultLauncher;

import org.json.JSONObject;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;

public class MainActivity extends AppCompatActivity {

    EditText etChannelId, etApiKey;
    TextView tvData;
    Button btnStart, btnQR;
    Handler handler = new Handler();
    Runnable fetchRunnable;

    boolean isFetching = false;

    private final ActivityResultLauncher<ScanOptions> qrLauncher =
            registerForActivityResult(new ScanContract(), result -> {
                if (result.getContents() != null) {
                    String qrContent = result.getContents();
                    Toast.makeText(this, "QR Scanned: " + qrContent, Toast.LENGTH_SHORT).show();

                    try {
                        JSONObject qrJson = new JSONObject(qrContent);
                        String channel = qrJson.optString("channel", "");
                        String key = qrJson.optString("key", "");
                        etChannelId.setText(channel);
                        etApiKey.setText(key);
                    } catch (Exception e) {
                        etChannelId.setText(qrContent);
                    }
                } else {
                    Toast.makeText(this, "Scan cancelled", Toast.LENGTH_SHORT).show();
                }
            });

    @SuppressLint("MissingInflatedId")
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        this.getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
                WindowManager.LayoutParams.FLAG_FULLSCREEN);
        getSupportActionBar().hide();
        setContentView(R.layout.activity_main);

        etChannelId = findViewById(R.id.etChannelId);
        etApiKey = findViewById(R.id.etApiKey);
        tvData = findViewById(R.id.tvData);
        btnStart = findViewById(R.id.btnStart);
        btnQR = findViewById(R.id.qr);

        StrictMode.setThreadPolicy(new StrictMode.ThreadPolicy.Builder().permitNetwork().build());

        btnStart.setOnClickListener(v -> {
            if (!isFetching) startFetching();
            else stopFetching();
        });

        btnQR.setOnClickListener(v -> openQRScanner());
    }

    private void openQRScanner() {
        ScanOptions options = new ScanOptions();
        options.setPrompt("Scan ThingSpeak QR Code");
        options.setBeepEnabled(true);
        options.setOrientationLocked(true);
        qrLauncher.launch(options);
    }

    private void startFetching() {
        String channelId = etChannelId.getText().toString().trim();
        String apiKey = etApiKey.getText().toString().trim();

        if (channelId.isEmpty()) {
            tvData.setText("Please enter a valid Channel ID.");
            return;
        }

        isFetching = true;
        btnStart.setText("Stop Fetching Data");
        tvData.setText("Starting data updates...");

        fetchRunnable = () -> {
            fetchData(channelId, apiKey);
            handler.postDelayed(fetchRunnable, 3000);
        };
        handler.post(fetchRunnable);
    }

    private void stopFetching() {
        isFetching = false;
        handler.removeCallbacks(fetchRunnable);
        btnStart.setText("Start Fetching Data");
        tvData.append("\n\nStopped fetching data.");
    }

    // 🔥 NEW FUNCTION: Detect motion based on x,y,z
    private String detectMotion(double x, double y, double z) {
        double magnitude = Math.sqrt(x * x + y * y + z * z);

        if (magnitude < 0.2) return "Drastic Fall Down";
        if (magnitude < 0.5) return "Fell Down";
        if (magnitude < 1.22) return "Normal Movement";
        return "Static / No Movement";
    }

    private void fetchData(String channelId, String apiKey) {
        try {
            String urlString = "https://api.thingspeak.com/channels/" + channelId + "/feeds/last.json";
            if (!apiKey.isEmpty()) urlString += "?api_key=" + apiKey;

            URL url = new URL(urlString);
            HttpURLConnection conn = (HttpURLConnection) url.openConnection();
            conn.setRequestMethod("GET");

            BufferedReader reader = new BufferedReader(new InputStreamReader(conn.getInputStream()));
            StringBuilder response = new StringBuilder();
            String line;

            while ((line = reader.readLine()) != null)
                response.append(line);

            reader.close();
            conn.disconnect();

            JSONObject json = new JSONObject(response.toString());

            double x = json.optDouble("field1", 0);
            double y = json.optDouble("field2", 0);
            double z = json.optDouble("field3", 0);

            String motion = detectMotion(x, y, z);

            runOnUiThread(() -> {
                String dataText = "Latest Data:\n"
                        + "X-axis: " + x
                        + "\nY-axis: " + y
                        + "\nZ-axis: " + z
                        + "\n\nMotion Activity:\n" + motion;
                tvData.setText(dataText);
            });

        } catch (Exception e) {
            runOnUiThread(() -> tvData.setText("Error: " + e.getMessage()));
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        handler.removeCallbacks(fetchRunnable);
    }
}
