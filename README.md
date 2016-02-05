# VBoxRecorder

VBoxRecorder is a Windows application to record lossless video of Virtualbox VMs. 
At the time of writing, VirtualBox supports recording webm video, but not lossless video.
This application allows you to do just that.

### Usage
 1. Run your virtualbox VM
 2. Open VBoxRecorder.exe
 3. Type `machines` to view the available VMs
 4. Type `select #` to select a VM to record (# is the index of the VM in the list, the machine name or ID are also allowed)
 5. Specify an output file using `outputfile C:/path/to/file.avi`
 6. Type `record` to start recording
 7. Press <kbd>ESC</kbd> to stop recording
 
To get a list of all available commands, use `help`
### Q&A
  - I receive the message "Failed to create VirtualBox client!"
    - Make sure you are using the correct VirtualBox version. The correct version is usually specified in the release notes. Feel free to create an issue when VBoxRecorder stops working for a newer version of VirtualBox.
  - I receive the message "Could not find frame buffer!"
    - This can occur when resizing the VirtualBox window. Fully close VirtualBox and VBoxRecorder, then start VirtualBox and wait for the OS to start. Then start VBoxRecorder and try to record.
  - When I make recordings, the video file seems to be distorted/corrupted?
    - Make sure that your VirtualBox screen has a regular desktop resolution. I couldn't find any numbers on this, but 1920x976, 1280x960, 1152x864, 1024x768 and 800x600 all worked for me. Using an unusual resolution causes distortion and corruption in the video codec, but PNG's should still work.
  - Why does this application need administrator rights?
    -   The VirtualBox API does contain a function "TakeScreenshot", however it seems to be broken. An alternative is available, but not fast enough for realtime high quality video capture. This application solves this problem by reading the framebuffer directly from the VM process memory. This requires administrator rights.
  - What formats are supported?
    -  Raw PNG sequence, HuffYUV (recommended), H265(broken?) and FFV1 (broken?)
       <br> You can switch between these with the `outputmode` command
       <br> Support for more formats could be added since this project uses libavcodec by the FFMPEG project for its video encoding.

### Libraries
VBoxRecorder uses a number of open source projects to work properly:
* [VirtualBox API] - Retrieve information about the VMs
* [libavcodec] - Used to encode videos (in lossless or lossy format)
* [LodePNG] - Supereasy writing of PNG images to disk

[VirtualBox API]: https://www.virtualbox.org/wiki/Technical_documentation
[libavcodec]: https://www.ffmpeg.org/libavcodec.html
[LodePNG]: http://lodev.org/lodepng/

### Scripting
By using VBoxRecorder in a script, you can skip the setup and go straight to recording if you pass the required arguments.<br>
The general syntax looks like this:<br>
```bat
VBoxRecorder.exe -pid PROCESS_ID -width WIDTH -height HEIGHT -bpp BITS_PER_PIXEL -machine VM_ID -outputmode OUTPUT_MODE_ID -outputfile OUTPUT_FILE_PATH
```
 - `PROCESS_ID`: The process ID of the VirtualBox.exe VM process. Search for the process containing the VM ID in its startup arguments. If there are multiple processes, pick the one with the largest memory footprint
 - `WIDTH`: The width of the VM screen
 - `HEIGHT`: The height of the VM screen
 - `BITS_PER_PIXEL`: The color depth of the VM screen. Only 32 bits per pixel is currently supported
 - `VM_ID`: the VirtualBox ID of the VM you want to record. Run `VBoxManage list vms` to find this ID
 - `OUTPUT_MODE_ID`: the outputmode(png, huffyuv, ...) that should be used in the recording. Start VBoxRecorder and run `outputmode` to view all available options.
 - `OUTPUT_FILE_PATH`: The path to the file to store the recording.
