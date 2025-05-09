After re-installing Windows on my Asus Flow x13, the trackpad no longer disabled when folding the screen back and entering tablet mode.

I created this service to fix the issue.

Installation steps:

1. Download the exe and place it somewhere convienient - e.g. C:\TrackpadHackService
2. Register the service - Open an elevated command prompt and enter 'sc create TouchpadHackService binPath= "PathToService.exe" start= auto'
3. Start the service - 'net start TrackpadHackService'

This will run the service automatically and mean you never have to think about it again

DISCLAIMER!!

I take no responsibiltiy for any issues or damage caused by this software. The functionality is basic and just disables the trackpad device via its hardware ID. 
If something goes wrong and you no longer have a working trackpad, you would most likely fix it by re-enabling the trackpad in device manager and then removing the service
