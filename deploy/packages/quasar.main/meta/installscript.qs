function Component()
{
    // default constructor
}

Component.prototype.createOperations = function()
{
    // call default implementation
    component.createOperations();

    if (systemInfo.productType === "windows") {
        component.addOperation("Execute", "@TargetDir@/vc_redist.x64.exe", "/install", "/passive", "/norestart");
    }
}
