==============================================================================================================
Prototype loadibec for Windows written by Dokugogagoji (no longer active). oibc on the way hopefully. Only for people who know what they are doing.
==============================================================================================================
1. Go to device manager and have the iPhone in recovery mode. Uninstall the apple mobile device driver (check delete driver as well). You should now have two different devices labelled apple or whatever.
2. Download libusb-win32 and install that while the phone is in recovery loop.
3. Find out which device is which:
	a.Right click
	b.Properties
	c.Details
	d.Hardware ID
	e.Note whether MI_00 or MI_01 is on the end of the first string
4. Now install the right driver for the right device:
	a.Right click
	b.Properties
	c.Driver
	d.Update Driver
	e.Browse my computer
	f.Let my pick
	g.Have Disk and navigate to the folder and select the correct one (multiple interfaces 0 --> MI_00)
	h.Click allow if it complains that the driver is not signed or whatever
	g.Repeat for the other device
5. Run loadibec.bat and watch it load :D
==============================================================================================================
This job is tedious and I do plan on talking to other devs (such as nitestarzz who has a windows auto-installer) in order to simplify the process.
==============================================================================================================
