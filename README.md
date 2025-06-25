# NAVNETDLL Installation Procedure

---

## 1. Installation

```bash
cd /
zcat NAVNETDLL_RADAR_Linux_32bit/64bit_xx.xx.tar.gz | tar -xvf -
```

---

## 2. File Composition

All files are installed under:

```text
/usr/lib/NAVNETDLL/
```

---

## 3. Compilation & Linking

The sample source code lives in:

```text
/usr/lib/NAVNETDLL/sample
```

> **NOTICE:** Make sure to include the NAVNETDLL link option during compilation.

### 3.1 Compile (generic)

```bash
cd /usr/lib/NAVNETDLL/sample
g++ -O2 -Wall -I../include \
    -c -o main.o Furuno_Sample.cpp \
    -DFLM_COMPILE_NEW_LICENSE=1

g++ -o main main.o \
    -Wall -lNAVNETDLL -L../lib \
    -lpthread -lm -ldl
```

### 3.2 Run (generic)

> Be careful with spacing in the command line.

```bash
export LD_LIBRARY_PATH=../lib
./main [vendor.txt path] [target.dat path] [IPv4 address] [hostname]
```

* **Example:**

  ```bash
  ./main vendor.txt target.dat 192.168.0.10 PC1
  ```

---

## 4. Specific for My Use Case

### 4.1 Compile

```bash
cd sample
sudo g++ -O2 -Wall -I../include \
    -c -o main.o Furuno_Sample.cpp \
    -DFLM_COMPILE_NEW_LICENSE=1

sudo g++ -o main main.o \
    -Wall -lNAVNETDLL -L../lib \
    -lpthread -lm -ldl
```

### 4.2 Run

```bash
export LD_LIBRARY_PATH=../lib
./main vendor.txt target.dat 172.31.1.100 PC1
```

---

## 5. What Is the `target.dat` File?

The `target.dat` file bundled with the NAVNETDLL package contains unique certification code(s) for supported radar sensor model(s). These codes whitelist which radar models can connect. If the code in `target.dat` does not match your sensor’s model, the connection will fail.

### 5.1 Storing `target.dat`

* Save `target.dat` locally.
* Pass its full path as the **third** argument when running `main`.
* **Required** for radar connection.

### 5.2 Connecting an Appropriate Radar

Ensure the radar sensor listed in `target.dat` is on the same network. If not, `Flm_certification` and/or `RmInitialModule` will fail.

### 5.3 Firewall Settings

If the sample program still cannot detect the radar:

1. Check whether your firewall is active.
2. Temporarily disable or configure the firewall to allow the connection.

---

## 6. Uninstallation

To remove NAVNETDLL completely:

```bash
cd /usr/lib
rm -rf NAVNETDLL
```
