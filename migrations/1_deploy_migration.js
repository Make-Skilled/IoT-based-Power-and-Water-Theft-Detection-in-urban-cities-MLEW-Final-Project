const userManagement=artifacts.require("userManagement");

module.exports=function(deployer){
    deployer.deploy(userManagement);
}