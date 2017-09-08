# UnrealCar

This repository stores a sample project that renders traffic activity in Unreal Engine from the Automotive Driving models.
It is likely that this whole project is compatible only with MacOS platform.

In order for the project to work in Linux, follow exactly these steps

1.) Make sure the Vehicle Vol.1 is re-installed. Some of the very large files are missing, though I guess it shouldn't hurt the performance much for the time being.
2.) Create a blank C++ project in Unreal Engine and name it "car"
3.) Exit the GUI
4.) Go to the project folder and browse to car/Source/car
5.) In this repository, browse to UnrealCar/car/Source/car
6.) Copy the following files into your car/Source/car:
    MainSceneActor.cpp
    MainSceneActor.h
    car.Build.cs
7.) Review changes need to be made in MainSceneActor.cpp, and car.Build.cs regarding the paths. (Search for "README")
8.) Activate the python and julia client that serves the actions
9.) Build the project and test run in Unreal Engine.
