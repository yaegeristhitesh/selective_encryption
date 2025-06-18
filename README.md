# IITJAMMU_24VI59IITJ_Selective_encryption_for_H_264_AVC_Video_streams
SRIB-PRISM Program

Project Setup and Usage

This README provides instructions on how to set up and use this project within a Docker container.
1. Start Docker

First, ensure Docker is installed and running on your system. You can verify the installation by checking its version:
Bash

docker --version

If Docker isn't running, please start Docker Desktop (for Windows/macOS) or your operating system's Docker runtime.
2. Open the Project in VS Code (Inside the Container)

To work with the project in a containerized environment:

    Open VS Code and load your project folder.
    Press F1 (or Ctrl+Shift+P) to open the command palette.
    Select the command: Remote-Containers: Reopen in Container

VS Code will then automatically build (if necessary) and launch the project inside a Docker container, providing a consistent development environment.
3. (Important) Using Your Own Video Files

If you plan to use your own video files for testing, you must copy or move them into the project folder before opening the project in the container. Files located outside the project directory are not accessible from within the Docker container by default.

For example, you can place your video file in the video/ directory:

video/myvideo.mp4

4. Run the Main Script

Once your project is open in the VS Code terminal (inside the container), you can run the main script using the following command:
Bash

./run video/test2.mp4 video/output/encrypted.mp4 video/output/decrypted.mp4

Here's a breakdown of the arguments:

    video/test2.mp4: This is the input video file.
    video/output/encrypted.mp4: This will be the output encrypted video file.
    video/output/decrypted.mp4: This will be the output decrypted video file.

Remember: You can replace test2.mp4 with the name of any video file you have placed inside the video/ directory.