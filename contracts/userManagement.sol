// SPDX-License-Identifier: MIT
pragma solidity >=0.4.22 <0.9.0;

contract userManagement {
    struct User {
        address _userAddress;
        string _userName;
        string _userPassword; // Will store hashed password
        string _userEmail;
        bool _exist;
    }

    struct Notification {
        address _userAddress;
        string _type;
        string _message;
        string _parameter;
        string _value;
        uint256 _timestamp;
        bool _isRead;
    }

    mapping(address => User) public users;
    mapping(string => User) public usernames;
    address[] public userAddresses;
    
    // Mapping from user address to their notifications
    mapping(address => Notification[]) private userNotifications;
    
    modifier userNotExists(address wallet, string memory username) {
        require(!users[wallet]._exist, "Wallet address is already registered");
        require(!usernames[username]._exist, "Username is already taken");
        _;
    }

    modifier userExists(address wallet) {
        require(users[wallet]._exist, "User does not exist");
        _;
    }

    function userSignUp(
        address wallet,
        string memory username,
        string memory hashedPassword,
        string memory email
    ) public userNotExists(wallet, username) {
        require(bytes(username).length >= 3, "Username must be at least 3 characters long");
        require(bytes(hashedPassword).length > 0, "Password hash cannot be empty");
        require(bytes(email).length > 0, "Email cannot be empty");
        require(wallet != address(0), "Invalid wallet address");
        require(msg.sender == wallet, "Sender must be the registering wallet");

        User memory newUser = User({
            _userAddress: wallet,
            _userName: username,
            _userPassword: hashedPassword,
            _userEmail: email,
            _exist: true
        });

        users[wallet] = newUser;
        usernames[username] = newUser;
        userAddresses.push(wallet);
    }

    function userLogin(string memory username, string memory hashedPassword) public view returns (bool) {
        require(usernames[username]._exist, "User does not exist");
        
        // Direct comparison of hashed passwords
        return keccak256(abi.encodePacked(usernames[username]._userPassword)) == 
               keccak256(abi.encodePacked(hashedPassword));
    }

    function viewUserByUsername(string memory username) public view returns (
        address,
        string memory,
        string memory,
        string memory
    ) {
        require(usernames[username]._exist, "User does not exist");
        User memory user = usernames[username];
        return (
            user._userAddress,
            user._userName,
            user._userPassword,
            user._userEmail
        );
    }

    // New notification functions
    function addNotification(
        address userAddress,
        string memory notificationType,
        string memory message,
        string memory parameter,
        string memory value
    ) public userExists(userAddress) {
        Notification memory newNotification = Notification({
            _userAddress: userAddress,
            _type: notificationType,
            _message: message,
            _parameter: parameter,
            _value: value,
            _timestamp: block.timestamp,
            _isRead: false
        });

        userNotifications[userAddress].push(newNotification);
    }

    function getUserNotifications(address userAddress) 
        public 
        view 
        userExists(userAddress)
        returns (
            string[] memory types,
            string[] memory messages,
            string[] memory parameters,
            string[] memory values,
            uint256[] memory timestamps,
            bool[] memory isRead
        ) 
    {
        Notification[] storage notifications = userNotifications[userAddress];
        uint256 length = notifications.length;

        types = new string[](length);
        messages = new string[](length);
        parameters = new string[](length);
        values = new string[](length);
        timestamps = new uint256[](length);
        isRead = new bool[](length);

        for (uint256 i = 0; i < length; i++) {
            types[i] = notifications[i]._type;
            messages[i] = notifications[i]._message;
            parameters[i] = notifications[i]._parameter;
            values[i] = notifications[i]._value;
            timestamps[i] = notifications[i]._timestamp;
            isRead[i] = notifications[i]._isRead;
        }

        return (types, messages, parameters, values, timestamps, isRead);
    }

    function getUnreadNotificationCount(address userAddress) 
        public 
        view 
        userExists(userAddress)
        returns (uint256) 
    {
        Notification[] storage notifications = userNotifications[userAddress];
        uint256 unreadCount = 0;
        
        for (uint256 i = 0; i < notifications.length; i++) {
            if (!notifications[i]._isRead) {
                unreadCount++;
            }
        }
        
        return unreadCount;
    }

    function markNotificationAsRead(address userAddress, uint256 notificationIndex) 
        public 
        userExists(userAddress) 
    {
        require(msg.sender == userAddress, "Only the user can mark their notifications as read");
        require(notificationIndex < userNotifications[userAddress].length, "Invalid notification index");
        require(!userNotifications[userAddress][notificationIndex]._isRead, "Notification already read");

        userNotifications[userAddress][notificationIndex]._isRead = true;
    }

    function markAllNotificationsAsRead(address userAddress) 
        public 
        userExists(userAddress) 
    {
        require(msg.sender == userAddress, "Only the user can mark their notifications as read");
        
        Notification[] storage notifications = userNotifications[userAddress];
        for (uint256 i = 0; i < notifications.length; i++) {
            if (!notifications[i]._isRead) {
                notifications[i]._isRead = true;
            }
        }
    }
}
