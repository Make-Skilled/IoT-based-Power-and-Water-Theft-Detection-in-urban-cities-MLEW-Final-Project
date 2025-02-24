// SPDX-License-Identifier: MIT
pragma solidity >=0.4.22 <0.9.0;

contract userManagement {
    struct User {
        address _userAddress; // Mandatory
        string _userName;
        string _userPassword;
        string _userEmail;
        bool _exist;
    }

    mapping(address => User) users; // user wallet address is pointing to a structure of user
    mapping(string => User) usernames; // _usernames[username]
    address[] userAddresses; // user dynamic array of wallet addresses

    // Create an account for the user
    function userSignUp(address wallet, string memory username, string memory password, string memory email) public {
        require(!users[wallet]._exist, "Account already exists");
        require(!usernames[username]._exist, "Username already taken");

        User memory newUser = User(wallet, username, password, email, true);
        users[wallet] = newUser;
        usernames[username] = newUser;
        userAddresses.push(wallet);
    }

    // Return all registered users
    function viewAllUsers() public view returns (User[] memory) {
        User[] memory userArray = new User[](userAddresses.length);

        for (uint256 i = 0; i < userAddresses.length; i++) {
            userArray[i] = users[userAddresses[i]];
        }

        return userArray;
    }

    // Return one record of user based on username
    function viewUserByUsername(string memory username) public view returns (User memory) {
        require(usernames[username]._exist, "No account with that username");
        return usernames[username];
    }

    // Return one record of user based on wallet address
    function viewUserByWallet(address wallet) public view returns (User memory) {
        require(users[wallet]._exist, "No account with that wallet");
        return users[wallet];
    }

    // Validate login details of user based on username and password
    function userLogin(string memory username, string memory password) public view returns (bool) {
        require(usernames[username]._exist, "No account with that username");

        // Check if password matches
        if (keccak256(bytes(password)) == keccak256(bytes(usernames[username]._userPassword))) {
            return true;
        } else {
            return false;
        }
    }
}
