# 🍓 Yocto Project based Custom BSP for Raspberry Pi 4

## 📖 Overview

**Yocto Project(Poky, Kirkstone)** 를 기반으로 라즈베리 파이 4용 **커스텀 임베디드 리눅스 이미지**를 구축하는 BSP(Board Support Package) 레이어(`meta-mylayer`)

https://github.com/Jminu/Driver/tree/master 프로젝트에서 개발된 **커널 드라이버, 애플리케이션, 디바이스 트리 오버레이**를 Yocto 레시피(Recipe)로 통합하여, 빌드 시 **자동으로 드라이버 포함, 서비스가 실행되는 완성된 OS 이미지** 생성

-----

## 🏗️ Layer 구조 (meta-mylayer)

```
meta-mylayer/
├── conf/
│   └── layer.conf             # 레이어 설정 및 우선순위 정의
├── recipes-app/
│   └── sensor-app/            # 유저 공간 모니터링 앱
│       ├── files/             # 소스코드 (app.c, systemd service)
│       └── sensor-app.bb      # 앱 빌드 및 Systemd 서비스 등록 레시피
├── recipes-bsp/
│   └── sensor-overlay/        # 디바이스 트리 오버레이
│       ├── files/             # dts 소스 (jmw-sht20.dts 등)
│       └── sensor-overlay.bb  # dtbo 컴파일 및 배포 레시피
└── recipes-kernel/
    └── sensor-drivers/        # 커스텀 디바이스 드라이버
        ├── files/             # 드라이버 소스 (sht20, hd44780, irq_btn)
        └── sensor-drivers.bb  # 커널 모듈 빌드(Out-of-tree) 및 오토로드 설정
```

-----

## 🛠️ Tech Stack & Features

| Category | Technology | Description |
| :--- | :--- | :--- |
| **Build System** | Yocto Project | Poky (Kirkstone branch), BitBake |
| **Target HW** | Raspberry Pi 4B | 64-bit Architecture (bcm2711) |
| **Kernel** | Linux Kernel Module | `inherit module`을 사용 모듈 빌드 |
| **Init System** | Systemd | 부팅 시 센서 모니터링 앱 자동 실행 서비스 등록 |
| **Device Tree** | Device Tree Overlay | `dtc` 컴파일 및 `/boot/overlays` 자동 배포 |

-----

## ✨ Key Integration Features

### 1\. Kernel Module Autoloading

  * **Recipe:** `recipes-kernel/sensor-drivers/sensor-drivers.bb`
  * **Feature:** `KERNEL_MODULE_AUTOLOAD` 변수를 사용하여, 리눅스 부팅 시 `sht20_driver`, `hd44780_driver`, `irq_btn_driver`가 **자동으로 `insmod` 되도록 구성**.

### 2\. Systemd Service Integration

  * **Recipe:** `recipes-app/sensor-app/sensor-app.bb`
  * **Feature:** `sensor-app.service` 파일을 작성하고 `inherit systemd` 클래스를 사용하여, 부팅 완료 후(multi-user.target) **모니터링 애플리케이션이 백그라운드 서비스로 자동 실행**되도록 설정.

### 3\. Device Tree Overlay Deployment

  * **Recipe:** `recipes-bsp/sensor-overlay/sensor-overlay.bb`
  * **Feature:** `jmw-sht20.dts`, `jmw-hd44780.dts`를 빌드 타임에 `.dtbo`로 컴파일하고, 최종 이미지의 `/boot/overlays/` 경로에 자동 배포.
  * **Config:** `local.conf`의 `RPI_EXTRA_CONFIG`를 통해 `config.txt`에 오버레이 적용 구문 자동 추가.

### 4\. Custom Image Configuration

  * **Config:** `conf/local.conf`
  * **Feature:** `ssh-server-openssh`, `i2c-tools`, `vim` 등 개발 필수 패키지 및 커스텀 레시피(`sensor-drivers`, `sensor-app`)를 이미지에 포함.
