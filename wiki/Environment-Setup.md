# System Execution & Deployment Guide

This guide is intended for testers and end users who want to set up and run the Drive application. It covers the environment setup, compilation via Docker, and how to use both the Web and Android clients.

---

## Project Structure

```
Drive/
├── src/
│   ├── web-server/        # Node.js/Express REST API
│   ├── react-client/      # React web frontend
│   ├── mobile-client/     # React Native (Expo) Android app
│   ├── Server/            # C++ TCP server source
│   ├── Client/            # C++ TCP client source
│   ├── Commands/          # C++ command implementations
│   ├── Services/          # C++ services (compression, storage)
│   └── Core/              # C++ core application logic
├── tests/                 # Test files
├── wiki/                  # Documentation (this guide)
├── docker-compose.yml     # Orchestration file
├── Dockerfile             # C++ server build
└── storage/               # File storage volume
```

---

## 1. Environment Setup & Compilation

The entire stack is containerized. Docker Compose will handle the compilation of the source code and the networking between services.

### Prerequisites

| Software | Version | Purpose |
|----------|---------|---------|
| **Docker Desktop** | 4.0+ | Container runtime |
| **Docker Compose** | Included with Desktop | Multi-container orchestration |
| **Git** | Latest | Version control |

### Verify Installation

```bash
docker --version
docker-compose --version
git --version
```

### Deployment Command

Open your terminal in the root directory and run:

```bash
docker-compose up --build
```

**What this does:**

- **Compiles the C++ code:** Triggers CMake build in the Dockerfile, generating the TCP server executable
- **Builds the React frontend:** Compiles the React client and bundles it with the web server
- **Creates Images:** Generates local images for the Backend, Frontend, and Database
- **Orchestrates:** Starts all containers in the correct order based on dependencies

> **Note:** Once the terminal shows "Server ready on port 3000", the environment is ready.

### Run in Background (Detached Mode)

```bash
docker-compose up -d --build
```

### Stop All Services

```bash
docker-compose down
```

---

## 2. Services Overview

The application consists of the following Docker services:

| Service | Container Name | Port | Description |
|---------|----------------|------|-------------|
| `mongodb` | mongodb-container | 27017 | MongoDB database for user data and file metadata |
| `server` | cpp-server-container | 8080 | C++ TCP server for file storage and RLE compression |
| `web-server` | web-server-container | 3000 | Node.js REST API + React frontend |

### Service Dependencies

```
mongodb (must be healthy)
    └── server (TCP Server)
    └── web-server (depends on both mongodb and server)
```

---

## 3. Accessing the Web Client

Once the services are running, open your browser and navigate to:

```
http://localhost:3000
```

**What you can do:**

- Register a new account
- Login with existing credentials
- Create, edit, and delete files and folders
- Share files with other users
- Search for files

---

## 4. Running the Android Mobile Client

The mobile client is built with React Native and Expo. It runs outside of Docker on your local machine.

### Prerequisites

| Software | Purpose |
|----------|---------|
| **Node.js** (>= 18) | JavaScript runtime |
| **npm** | Package manager |
| **Expo Go App** | Run on physical device |
| **Android Studio** | Android emulator (optional) |

### Step 1: Ensure Backend is Running

```bash
docker-compose up -d
```

Verify the web server is accessible:

```bash
curl http://localhost:3000/health
```

### Step 2: Install Dependencies

```bash
cd src/mobile-client
npm install
```

### Step 3: Start Expo Development Server

```bash
npm start
```

This will display a QR code and menu with options.

### Step 4: Run on Device/Emulator

#### Option A: Physical Device (Recommended)

1. Install **Expo Go** app from Play Store
2. Scan the QR code displayed in the terminal
3. The app will load on your device

> **Note:** Your phone and computer must be on the same Wi-Fi network.

#### Option B: Android Emulator

1. Open Android Studio
2. Go to **Tools > Device Manager**
3. Start an Android Virtual Device (AVD)
4. Press `a` in the Expo terminal or run:

```bash
npm run android
```

#### Option C: Web Browser

1. Press `w` in the Expo terminal or run:

```bash
npm run web
```

2. The app will open in your default web browser

> **Note:** The web version uses `localhost` to connect to the API, so ensure the backend is running on `localhost:3000`.

### API Connection (Automatic)

The mobile client automatically detects the correct API URL:

| Environment | API URL |
|-------------|---------|
| Physical Device | `http://<your-computer-ip>:3000/api` |
| Android Emulator | `http://10.0.2.2:3000/api` |

---

## 5. Environment Variables

Customize the services by creating a `.env` file in the project root:

```env
# MongoDB Configuration
MONGODB_PORT=27017
MONGODB_DATABASE=drive_db

# TCP Server Configuration
SERVER_PORT=8080
THREAD_POOL_SIZE=20

# Web Server Configuration
WEB_PORT=3000
JWT_SECRET=your-secret-key
JWT_EXPIRATION=24h
```

### Variable Reference

| Variable | Default | Description |
|----------|---------|-------------|
| `MONGODB_PORT` | 27017 | MongoDB exposed port |
| `MONGODB_DATABASE` | drive_db | Database name |
| `SERVER_PORT` | 8080 | TCP server port |
| `THREAD_POOL_SIZE` | 20 | TCP server thread pool size |
| `WEB_PORT` | 3000 | Web server port |
| `JWT_SECRET` | dev-secret-for-testing-only | JWT signing secret |
| `JWT_EXPIRATION` | 24h | JWT token expiration time |

---

## 6. Data Persistence

### Volumes

| Volume | Purpose |
|--------|---------|
| `mongodb_data` | MongoDB database files |
| `mongodb_config` | MongoDB configuration |
| `./storage` | TCP server file storage (local mount) |

### Clear All Data

> **Warning:** This permanently deletes all stored data.

```bash
docker-compose down -v
rm -rf ./storage/*
```

---

## 7. Viewing Logs

### All Services

```bash
docker-compose logs -f
```

### Specific Service

```bash
docker-compose logs -f web-server
docker-compose logs -f server
docker-compose logs -f mongodb
```

---

## 8. Troubleshooting

### Container Won't Start

Check container logs:

```bash
docker-compose logs <service-name>
```

### Port Already in Use

Free the port:

**Windows:**
```bash
netstat -ano | findstr :3000
taskkill /PID <PID> /F
```

**macOS/Linux:**
```bash
kill -9 $(lsof -ti:3000)
```

**Or stop Docker containers:**
```bash
docker-compose down
```

### MongoDB Connection Issues

Ensure MongoDB is healthy:

```bash
docker-compose ps
```

The `mongodb` service should show `healthy` status.

### Mobile Client Cannot Connect

1. **Verify services are running:**
   ```bash
   docker-compose ps
   ```

2. **For physical devices**, ensure:
   - Phone and computer are on the same Wi-Fi network
   - Firewall allows connections on port 3000

3. **Find your computer's IP:**
   - **Windows:** `ipconfig` (look for IPv4 Address)
   - **macOS/Linux:** `ifconfig` or `ip addr`

4. **Test from device:** Open browser on your phone and navigate to `http://<your-ip>:3000/health`

### Reset Everything

```bash
docker-compose down -v
docker-compose build --no-cache
docker-compose up -d
```

---

## 9. Health Checks

### Check Service Status

```bash
docker-compose ps
```

### Web Server Health

```bash
curl http://localhost:3000/health
```

### MongoDB Health

```bash
docker exec mongodb-container mongosh --eval "db.adminCommand('ping')"
```

---
