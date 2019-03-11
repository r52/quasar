function Component()
{
    // default constructor
}

Component.prototype.createOperations = function()
{
    // call default implementation
    component.createOperations();

    if (systemInfo.productType === "windows") {
        component.addOperation("CreateShortcut", "@TargetDir@/Quasar.exe", "@StartMenuDir@/Quasar.lnk",
            "workingDirectory=@TargetDir@");
        component.addOperation("CreateShortcut", "@TargetDir@/maintenancetool.exe", "@StartMenuDir@/Uninstall Quasar.lnk",
            "workingDirectory=@TargetDir@");
    }
}
