var configuration,lastdata;
Pebble.addEventListener("ready",
                        function(e) {
                          //setEndpoint(); TODO!!! CHANGE
                          localStorage.endpoint = "http://tomas-pebble.kbc-devel-02.keboola.com/app_dev.php/pebble";
                          configuration = {};                          
                          if (localStorage.configuration) 
                            configuration = JSON.parse(localStorage.configuration);
                            else
                            configuration = {
                              "connection":"on",
                              "token":""                              
                            }; 
                          
                          if (localStorage.lastdata) 
                            lastdata = JSON.parse(localStorage.lastdata);
                            else
                            lastdata = {                              
                              "0": " ",
                              "1": "No Data",
                              "2":" ",
                              "3":" "                              
                            }; 
                          
                          
                          console.log("configuration loaded!" + JSON.stringify(configuration));            
                          console.log("lastdata loaded!" + JSON.stringify(lastdata));   
                          sendAndStore(lastdata);                      
                         
                        });

function setEndpoint()
{
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
            console.log("pebble backend url set to:" + localStorage.endpoint);
            }
        });
      }
    }
  };
  req.send(null);
  
}

function sendAndStore(what)
{
  //console.log("sending " + JSON.stringify(what));
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


function fetchData()
{
  //if we setup active connection to off we do nothing OR we dont have token setup yet
  if(configuration.connection !== "on" || configuration.token === "")
    {
      if ( configuration.token === "")
        Pebble.showSimpleNotificationOnPebble("Connection Error", "Storage api token is not set");
      return;    
    }
 
  var req = new XMLHttpRequest();
  var response;
 

  
  req.open('GET', localStorage.endpoint + "/stats/1", true);  
  req.setRequestHeader('X-iot-Token', configuration.token);
  req.timeout = 60000;
  
  console.log("request sent");
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
    configurl = "https://s3.amazonaws.com/kbc-apps.keboola.com/pebble/pebble-config.htm?token="+ configuration.token +"&connection=" + configuration.connection;
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
                                           localStorage.configuration= JSON.stringify(configuration);   
                                           fetchData();
                                         
                                         }
                                         else
                                         {
                                            console.log("setup got cancelled");
                                         }
                                     
                                     });