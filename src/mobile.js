var configuration,lastdata;
var endpoint;

Pebble.addEventListener("ready",
                        function(e) {
                          //setEndpoint(); TODO!!! CHANGE
                          setEndpoint();
                          
                          configuration = {};                          
                          if (localStorage.configuration) 
                            configuration = JSON.parse(localStorage.configuration);
                            else
                            configuration = {
                              "connection":"on",
                              "token":"",      
                              "name":"",
                              "email":"email"
                            }; 
                          
                          if (localStorage.lastdata) 
                            lastdata = JSON.parse(localStorage.lastdata);
                            else
                            lastdata = {                              
                              "0": "Connected",
                              "1": "Please register",
                              "2": "to display data.",
                              "3": "",
                              "4": "",
                              "5": "",
                              "6": "",
                            }; 
                          
                          
                          console.log("configuration loaded!" + JSON.stringify(configuration));            
                          console.log("lastdata loaded!" + JSON.stringify(lastdata));   
                          sendAndStore(lastdata);  
                          //initCommunication();
                          fetchData();
                         
                        });

function setEndpoint()
{   
 // return "http://tomas-pebble.kbc-devel-02.keboola.com/app_dev.php/pebble";
  var req = new XMLHttpRequest();
  var response;
  req.open('GET', "https://connection.keboola.com/v2/storage", false);  
  req.onload  = function (e) {
    console.log ("onload triggered: ");
    if (req.readyState == 4) {
      if (req.status == 200 && req.responseText)
      {       
        response =  {};              
        response = JSON.parse(req.responseText);
        if(response.components)
          response.components.forEach(function(component) {
          if(component.id == "pebble")    
            {
            localStorage.endpoint = component.uri;
            endpoint = component.uri;
            console.log("pebble backend url set to:" + localStorage.endpoint);
            }
        });
      }
    }
     else 
      {
        console.log("Error setting endpoint " + req.status);
        Pebble.showSimpleNotificationOnPebble("Connection Error", "failed connect to KBC(" + req.status.toString() + ")");
      }
  };
  req.send(null);
  
}

function initCommunication()
{
    Pebble.sendAppMessage(
      {"400":2},
  function(e) {    
    console.log("Communication initialized");
      
  },
  function(e) {
    console.log("Unable to init communication. Error is: " + e.error.message);  
      
  });
  
}

function sendAndStore(what)
{
  
    Pebble.sendAppMessage(
      what,
  function(e) {    
      
  },
  function(e) {
    console.log("Unable to deliver message with transactionId=" + e.data.transactionId + " Error is: " + e.error.message);  
      
  }
);
  
  lastdata = what;  
  localStorage.lastdata = JSON.stringify(lastdata);     
}

function giveMeEndpoint()
{
  return endpoint;
 
}
function fetchData()
{
  
  //if we setup active connection to off we do nothing OR we dont have token setup yet
  if(configuration.connection !== "on" || configuration.token === "")
    {
      if ( configuration.token === "")
        Pebble.showSimpleNotificationOnPebble("Connection Error", "Access token is not set");
      return;    
    }
 
  var req = new XMLHttpRequest();
  var response;
  var getstatsurl = giveMeEndpoint() + "/stats/" + Pebble.getAccountToken();
  
  req.open('GET',getstatsurl , true);  
  req.setRequestHeader('X-iot-Token', configuration.token);
  req.timeout = 60000;
  
  console.log("request sent to " + getstatsurl);
	req.onload  = function (e) {
    console.log ("onload triggered: ");
    if (req.readyState == 4) {
      if (req.status == 200 && req.responseText)
      {        
        console.log("GOT FROM SERVER:" + req.responseText);        
        response =  {};              
        response = JSON.parse(req.responseText);        
        if (response == {} )
          return;
        
        var maxchanged = "";        
        var result = {};
        for (var i in response) {
           var row = response[i];
           result[row.rowid] = row.value;
           var page = i/6;
           var drowid = 200 + page;
           if((i % 6) === 0)
             {
               maxchanged = row.changed;
               result[drowid.toString()] = maxchanged;
             }
            else
              {
                if(maxchanged < row.changed)
                  {
                    maxchanged = row.changed;                                        
                    result[drowid.toString()] = maxchanged;
                  }
              }
         }          
       
        sendAndStore(result);
               
      }
      else 
      {
        console.log("Error " + req.status);
        Pebble.showSimpleNotificationOnPebble("Connection Error", "could not load data from the server, status=" + req.status.toString());
      }
    }
	};
  
  req.send(null);
  
}

Pebble.addEventListener("appmessage",
                        function(e) {
                          //console.log("message recieved");                               
                          if ( Pebble.getAccountToken() !== "")
                            fetchData();
                        });

Pebble.addEventListener("showConfiguration", function(e){
 // console.log("configuration is " + configuration);
if(configuration)
  {
    console.log("CONFIGURATION IS:" +JSON.stringify(configuration));
    var configurl;
    configurl = "https://s3.amazonaws.com/kbc-apps.keboola.com/pebble/pebble-config.htm?token="+ configuration.token +"&connection=" + configuration.connection+"&name=" + configuration.name+"&email=" + configuration.email;
    console.log("openning config URL:" + configurl);
Pebble.openURL(configurl);
  }
  
  
});


Pebble.addEventListener("webviewclosed",
                                    function(e) {                                       
                                       var setup;
                                       setup = JSON.parse(e.response);
                                       if(setup && Object.keys(setup).length > 0)
                                         {
                                           console.log("got reponse:" + e.response);                                    
                                           configuration.token = setup.token;
                                           configuration.connection = setup.connection;
                                           configuration.name = setup.name;
                                           configuration.email = setup.email;
                                           localStorage.configuration= JSON.stringify(configuration);   
                                           
                                           
                                           
  var watchid = Pebble.getAccountToken();
  if(watchid === "")
    {
    Pebble.showSimpleNotificationOnPebble("Register error", "User not logged into Pebble Watch.");
    return;
    } 
                                           
  var req = new XMLHttpRequest();  
  var string = JSON.stringify(
      {
        "name":configuration.name,
        "email":configuration.email,
        "connected":configuration.connection
      }
   );     
  console.log("registering with " + string);                                           
  console.log("registering to " + giveMeEndpoint() + "/watches/" + watchid);
  req.open('POST', giveMeEndpoint() + "/watches/" + watchid, true); 
  req.setRequestHeader('X-iot-Token', configuration.token);
  req.setRequestHeader('Content-type','application/json; charset=utf-8');
  req.setRequestHeader("Content-length", string.length);                                           
  req.onload  = function (e) {
    console.log ("onload triggered post watch: ");
    if (req.readyState == 4) {
      if (req.status == 200)
      {       
        console.log("status is 200:should be registered fetching data..");
        sendAndStore(
          {
          "0":"Connected",
          "1":"Successfully",
          "2":"registered!",
          "3":"Waiting for data",
          "4":"",
          "5":"",
          "6":""
          }
          );
        fetchData();
       
      }
      else 
      {
        console.log("Error " + req.responseText);
        Pebble.showSimpleNotificationOnPebble("Register Error", "Could not register user,contact support@keboola.com" + req.status.toString());
        sendAndStore({"0": "Connected",
                      "1": "Please register",
                      "2": "to display data.",
                      "3": "",
                      "4": "",
                      "5": "",
                      "6": ""});
      }
    }
  };
  req.send(string);
                                         
   }
 else
                                         {
                                            console.log("setup got cancelled");
                                         }
                                     
                                     });