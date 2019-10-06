# SmartThings Integration

Personal repository for SmartThings integration. look at the file names to file what is required for a set function. For example, files named similar to 'esp-switch' are all the files related to this functionalties.

Please read further to find a bit of introduction to each project. As I mentioned ealier, these are personal projects and not really intended for general avilabllity because I do not put enough time to test them for important bits that is required for commercial level products(such as security considerations)

## ESP-Switch
### Little history..
Please jump to the next sub-section if you do not want to read unnecessary information

The project started when all the Sonoff(and other ESP8266 smart switches for that matter) integrations I could find fall short of my requirement. Most of the custom firmware was designed for a borader appeal, which is great for people who like to tinker with things but not necessariliy the best implementation for SmartThings integration.

After spending countless hours trying to workaround issues with existing tool, I finally threw in the towel and decided to go full custom.

Fortunately, I am a Software Engineer myself and had done some robotics in my early adulthood as a hobby(for example, I was making 0-60mph timer for fun). Bringing back some of those memories after inhale soldering smoke was the easy part.

The goal was to use a custom firmware for a Sonoff device but I did not want to do anything too technical from the surface. For example, a lot of other fireware integration asks things like static IP address, manual device addition, etc. And even after all that, the whole status update is very clunky and often making things too slow.

### What it does
If you open the smart app after it is published, it will automatically search local Sonoff devices that have the custom firmware you can find here. Once discovered, it will list them on the screen for a user to pick devices to install. At the end of the process, new devices are created as a switch device.
These switch devices will have generic functionalities like On and Off. But also gets updated when the switch status is changed at the device side. Unlike polling status every so often(which also causes the hub to slow down somewhat after adding many switches), the status will be updated through SmartApp's API endpoint, which means that the status will be pushed from the device.
Meanwhile, SmartApp will run the discovery in the scheduled frequency to find if there are devices that has different IP address from when it was added. If such a device is detected, it will update the device record in SmartThings and they are synced once again.

### What is required
Required
  UPnP enabled local network
  Arduino
  USB Serial Programmer(and some wires to connect to Sonoff or ESP smart switch)

Optional(These are not good to have, you need them depending on the device you are using)
  Soldering iron and wire


### How to install
Important: This installation document is based on Sonoff Basic R2 device. Other ESP8266 based smart switches may use different pin and switch type and not compatible with the arduino sketch

There are three pieces of software that needs to be installed in different locations
1) SmartThings SmartApp
2) SmartThings Device handler
3) Arduino Sketch for ESP8266

- Go to SmartThings IDE site. (https://graph.api.smartthings.com/) Note that the site url may vary depending on your region.
- Open My SmartApps
- New SmartApp at the upper right hand corner
- Select From Code tab and leave the browser as it is
- Find SmartApp code from this repository https://github.com/pnjacket/SmartThings/tree/master/smartapps/djsmartapp/esp-switch-smartapp.src The file name is 'esp-switch-smartapp.groovy'. Open it by clicking it
- Copy contents of it and paste it to the SmnartApp tab that you left open
- Press Create
- Go back to MySmartApp and find the one that is just added. Click second small icon that has hover message 'Edit Properties'
- Click OAuth and Enable OAuth. Click Update
- Back to My SmartApps, now click the app name link
- Click Publish button and select For Me
- Now Open My Device Handlers on the top menu
- Create New Device Handler at the upper right hand corner
- Select From Code tab and leave the browser as it is
- Find Device Handler code from this repository https://github.com/pnjacket/SmartThings/tree/master/devicetypes/djdevice/esp-switch.src The file name is 'esp-switch.groovy' Open it by clicking it
- Copy contents of it and paste it to the Device handler tab that you left open
- Press Create
- Back to My Device Handlers, open the device handler by clicking the name
- Click Publish and select For Me
- Your job in SmartThings IDE is done!

I would like to suggest you to watch an Youtube Video explaning how to setup Arduino IDE for ESP8266
https://www.youtube.com/watch?v=2DL8FlrBTDs This is what I have used when I started.

- Once Arduino IDE is setup, download the sketch from https://github.com/pnjacket/SmartThings/tree/master/esp-switch The file name is 'esp-switch.ino'
- Connect Sonoff using a USB Serial Programmer
- Open the sketch and click 'Upload'. if this is successful, you can go to the next step.
- Make sure Sonoff device is restarted. You can unplug and plug it back to do this.
- Using a wifi enabled device(your cellphone or a computer) find a hotspot that is named similar to 'ESPSwitch0a55d1'. Last 6 characters are unique per device
- Connect to the said Wifi device
- Once connected go to http://192.168.4.1
- It will display a setup page and asking three pieces of information. Add necessary information and click submit. Note that only 2.4GHz network is supported
- Once submit and the information seems good, the device will restart in Operation mode and automatically join the network that you have specified.
- For whatever reason your wifi does not connect, you can press and hold the button for 3 seconds to go back to the setup mode
- Open SmartThings classic app(I have not tested the new Connect App)
- Open Automation
- Add a SmartApp
- You can find My Apps at the end of the list
- Select ESP Switch SmartApp
- The device discovery will start automatically. At this point follow the instruction on the screen.
- Enjoy!

### Future plans
As I mentioned earlier, this whole thing was developed to satisfy my requirements. Source will be updated as I needed but if changes that I need requires too much of custom code that does not apply to others, I will fork my code and work on that branch.
If you want to contribute to the project, please do so by putting pull request.

