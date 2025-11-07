# Fall Detection IoT Project (Arduino UNO + ADXL345 + ESP-01)

This project detects falls using an **ADXL345 accelerometer** connected to an **Arduino UNO**, and sends Z-axis readings plus fall alerts to **ThingSpeak** via an **ESP-01 Wi-Fi module**.

---

## 🧰 Hardware Setup

| Component | Pin Connection (UNO) | Notes |
|------------|----------------------|-------|
| ADXL345 (SDA) | A4 | I²C data line |
| ADXL345 (SCL) | A5 | I²C clock line |
| ESP-01 (TX) | D2 | SoftwareSerial RX |
| ESP-01 (RX) | D3 | SoftwareSerial TX (use level shifter) |
| ESP-01 (VCC, CH_PD, RST) | 3.3 V | Requires stable 3.3 V source |
| ESP-01 (GND) | GND | Common ground |

---

## ⚙️ ThingSpeak Setup

1. Go to [ThingSpeak → Channels → New Channel](https://thingspeak.com/channels/new).  
2. Enable **Field 1** = “Z-Axis (g)” and **Field 2** = “Fall Detected (1/0)”.  
3. Save the channel and copy your **Write API Key**.  
4. Paste it into your sketch:
   ```cpp
   const char* THINGSPEAK_API_KEY = "YOUR_WRITE_API_KEY";
   ```
5. Open **Serial Monitor** (9600 baud) and confirm:
   ```
   → ThingSpeak update OK
   ```
6. View the live graph on your channel’s **Private View** tab.

---

## 🧪 Sensitivity Settings

Adjust these constants in the code to change how easily a fall triggers:

```cpp
const float FREEFALL_THR = 0.9;  // higher = easier to trigger
const float IMPACT_THR   = 1.2;  // lower = easier to trigger
```

---

## 💻 Git & Version-Control Workflow

### 🧩 Clone the repository
```bash
git clone https://github.com/YOUR_USERNAME/fall-detection-arduino.git
cd fall-detection-arduino
```

### 🌿 Create a new branch for your changes
```bash
git checkout -b feature-name
# Example:
# git checkout -b led-alert
```

### ✏️ Make edits and commit
```bash
git add .
git commit -m "Added LED alert feature"
```

### 🚀 Push your branch to GitHub
```bash
git push -u origin feature-name
```

---

## 🔄 Merging changes into `main`

1. Go to your GitHub repo.  
2. You’ll see a banner: **“Compare & pull request”** → click it.  
3. Review the changes → click **“Merge Pull Request”** → **Confirm**.  
4. Back in your local folder, switch to `main` and update:
   ```bash
   git checkout main
   git pull origin main
   ```
5. (Optional) Delete the old branch if it’s merged:
   ```bash
   git branch -d feature-name
   git push origin --delete feature-name
   ```

---

## 🔁 Pulling the latest code into your working branch

If someone else updated `main` and you want those changes in your branch:
```bash
git checkout feature-name
git pull origin main
```
Resolve any merge conflicts if prompted, then:
```bash
git add .
git commit -m "Merged latest main updates"
git push
```

---

## 🧾 Recommended `.gitignore`
Add this file to prevent uploading compiled artifacts:

```
*.hex
*.elf
*.bin
*.log
*.tmp
.DS_Store
__MACOSX/
build/
```