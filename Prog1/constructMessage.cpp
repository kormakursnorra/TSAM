
// static uint32_t secretNumber;
// const char* userNames = "aroni21, bergurpb24, kormakur24";

// static std::string secretMessage;

// void constructMessage()
// {
//     std::random_device rd;
//     std::mt19937 gen( rd() ); 
//     std::uniform_int_distribution< uint32_t > dist( 0, UINT32_MAX );
    
//     secretNumber = dist( gen );
    
//     secretMessage.clear();
//     secretMessage.push_back( 'S' );

//     uint32_t netOrder = htonl(secretNumber); // convert to network byte order
//     // reinterpret_cast netOrder int value to char* to comply with append parameter
//     secretMessage.append( reinterpret_cast< const char* >( &netOrder ), sizeof( netOrder) ); 
//     secretMessage += userNames;
// }