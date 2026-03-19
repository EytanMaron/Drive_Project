FROM gcc:latest

# Install CMake
RUN apt-get update && apt-get install -y cmake

# Set the working directory
WORKDIR /usr/src/project

# Copy the entire project context
COPY . .

# Create a build directory and switch to it
WORKDIR /usr/src/project/build

# Configure and compile the project
# This generates both './myApp' and './myTests' executables
RUN cmake .. && make

# Default command (can be overridden by docker-compose)
CMD ["./myApp"]