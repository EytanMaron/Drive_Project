# Drive Web Application

Drive is a web application built for the Advanced Programming course.
It consists of a **React Native mobile client**, a **React web frontend**, a **Node.js/Express REST API** (web server) that exposes a Google Drive-like interface, a **MongoDB database** for persistent data storage, and a **C++ TCP server** that handles file storage and compression using run-length encoding (RLE).

The web server provides a RESTful API for user management, file/folder operations, permissions, and search functionality. **MongoDB** is used for persistent storage of user data, file metadata, file content, and permissions. The codebase is structured with loose coupling, SOLID principles, and MVC architecture in mind, making future extensions straightforward.

## Architecture Overview

The system consists of four main components:

1. **MongoDB Database** – Persistent storage for:
   - User data (authentication, profiles)
   - File metadata (name, type, parent folder, owner, etc.)
   - Permissions (read/write access for users)

2. **Web Server** (Node.js/Express) – RESTful API server that:
   - Connects to MongoDB for data persistence
   - Handles authentication with JWT tokens
   - Manages file/folder CRUD operations
   - Coordinates with TCP server for file operations

3. **TCP Server** (C++) – Handles file storage and compression using RLE

4. **React Native Mobile Client** – Mobile application for Android


## Documentation

For detailed instructions on using the application, see the [Wiki Documentation](./wiki/README.md):

- **[Environment Setup](./wiki/Environment-Setup.md)** – Complete guide to building and running the entire system 
- **[Login and Registration](./wiki/Login-and-Registration.md)** – User authentication and account management
- **[File Management](./wiki/File-Management.md)** – Creating, editing, and deleting files/folders
