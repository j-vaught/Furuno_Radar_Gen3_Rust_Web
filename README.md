**NAVNETDLL Installation Guide**

---

## Prerequisites

* **Operating System:** Linux ARM (32- or 64-bit)
* **Tools:** `git`, `unzip`, `tar`, `g++`, `sudo` (for system installs)
* **Permissions:** Ability to create directories under `/usr/lib` and adjust library paths.

---

## Quickstart (Simple Setup)

Choose one of the methods below; all dependencies are bundled, no additional downloads required.

### 1a. Option 1: Clone the Repository via Git

```bash
sudo git clone https://github.com/j-vaught/RADAR_web.git /usr/lib/NAVNETDLL
```

For immediate use, use the following command with the PAT(Personal Access Token). Expires Oct. 26, 2025
```bash
sudo git clone https://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@github.com/j-vaught/RADAR_web.git /usr/lib/NAVNETDLL
```

### 1b. Option 2: Download & Install via ZIP

1. Download the ZIP archive: [Download ZIP from GitHub](https://github.com/j-vaught/RADAR_web/archive/refs/heads/main.zip)
2. Create the target directory:

   ```bash
   sudo mkdir -p /usr/lib/NAVNETDLL
   ```
3. Unzip and flatten:

   ```bash
   sudo unzip ~/Downloads/RADAR_web-main.zip -d /usr/lib/NAVNETDLL
   sudo mv /usr/lib/NAVNETDLL/RADAR_web-main/* /usr/lib/NAVNETDLL/
   sudo rmdir /usr/lib/NAVNETDLL/RADAR_web-main
   ```

### 2. Verify Files

```bash
ls -l /usr/lib/NAVNETDLL
```

### 3. Install Boost (WebServer)

```bash
sudo apt-get update
sudo apt-get install libboost-all-dev
```

### 4. Set Ownership

Make your user the owner of the sample directory so you can edit files later:

```bash
sudo chown -R $USER:$USER /usr/lib/NAVNETDLL/sample
```

### 5. Run the Sample

```bash
cd /usr/lib/NAVNETDLL/sample
sh run.sh
```

* This script compiles, links, and runs the sample automatically.
* The web server will start at [http://localhost:8080](http://localhost:8080).

---

## Configuration

### Understanding `target.dat`

* Contains certification codes for approved radar sensor models.
* **Third argument** when running the sample: full path to `target.dat`.
* If the code doesn’t match your radar, connection will fail.

### Firewall & Network

1. Ensure radar and host are on the same network segment.
2. If connectivity issues persist, verify or temporarily disable firewall rules.

---

## Network Settings

To communicate with the radar, ensure the radar is powered on and your Raspberry Pi’s network is configured properly. Below are two ways to configure your IP address.

### Using the Graphical Network Manager (Recommended)

1. **Open the Network Menu**  
   Click on the Wi-Fi or Internet icon on the taskbar.  
   - Select **Advanced Options** (or **Network Settings**).  
   - Choose **Ethernet** connection or the interface you plan to use.  
   - Click the **Settings** icon.

2. **Set IPv4 to Static**  
   - Navigate to the **IPv4** tab.  
   - Click **Add**.  
   - Set **Address** to `172.31.1.100`.  
   - Set **Netmask** to `255.255.255.0` *(or `255.255.0.0` if your radar requires a Class B subnet)*.  
   - Set **Gateway** to `172.31.1.1` or whatever your radar requires.  

3. **Save and Apply**  
   Ensure IPv4 addressing is **Required** (or set to **Manual**).
   
---

## Uninstallation

To remove NAVNETDLL completely:

```bash
sudo rm -rf /usr/lib/NAVNETDLL
```

---

## Troubleshooting

* **Missing dependencies:** Ensure `g++`, `tar`, `unzip`, and `git` are installed.
* **Permission denied:** Confirm you have `sudo` privileges for `/usr/lib/NAVNETDLL`.
* **Library load errors:** Check `LD_LIBRARY_PATH` points to `/usr/lib/NAVNETDLL/lib`.
* **Connection failures:** Validate the `target.dat` codes and network/firewall settings.
