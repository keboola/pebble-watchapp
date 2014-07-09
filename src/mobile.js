var configuration,lastdata;
Pebble.addEventListener("ready",
                        function(e) {
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
                              "heading": "N/A",
                              "daystats": "N/A",
                              "weekstats":"N/A",
                              "datechange":"N/A"
                              
                            }; 
                          
                          
                          console.log("configuration loaded!" + JSON.stringify(configuration));            
                          console.log("lastdata loaded!" + JSON.stringify(lastdata));   
                          sendAndStore(lastdata);                      
                         
                        });
function sendAndStore(what)
{
  console.log("sending " + JSON.stringify(what));
    Pebble.sendAppMessage(
      {
            "HEADING":what.heading,
            "DAYSTATS":what.daystats ,
            "WEEKSTATS":what.weekstats,
            "DATECHANGE":what.datechange
        });
  
     lastdata.heading = what.heading;
     lastdata.daystats = what.daystats;
     lastdata.weekstats = what.weekstats;
     lastdata.datechange = what.datechange;
     lastdata.number1 = what.number1;
     lastdata.number2 = what.number2;
  
  localStorage.lastdata = JSON.stringify(lastdata);   
  
     
}

// Arguments: number to round, number of decimal places
function roundNumber(rnum, rlength) { 
    var newnumber = Math.round(rnum * Math.pow(10, rlength)) / Math.pow(10, rlength);
    return newnumber;
}

function isNumber(n) {
    return !isNaN(parseFloat(n)) && isFinite(n);
  }


function myPercentFormat(number){
  if(isNumber(number))
    {
      var res = roundNumber(number,2);
      var sign = res?res<0?-1:1:0;
      var signStr = "";
      if(sign > 0)
        signStr = "+";
      if(sign < 0)
        signStr = "-";
      return signStr + res.toString();
    }
  return number;
}

function  NA(val)
{
  if(val === undefined || val ===null)
    {
      return "n/a";
    }
  return val;
}
function fetchData()
{
  //if we setup active connection to off we do nothing OR we dont have token setup yet
  if(configuration.connection !== "on" || configuration.token === "")
    return;    
 
  var req = new XMLHttpRequest();
  var response;
  var heading, daystats, weekstats, datechange;

  
  req.open('GET', "http://pebble-rc.kbc-devel-02.keboola.com/app_dev.php/pebble/stats", true);  
  req.setRequestHeader('X-StorageApi-Token', configuration.token);
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
        
        heading = NA(response.heading);
        datechange = new Date(response.changed).toLocaleString();
        daystats =  NA(response.title1) + " (" + myPercentFormat(NA(response.percent1))  + "%)\n" + NA(response.number1);
        weekstats = NA(response.title2) + " (" + myPercentFormat(NA(response.percent2))  + "%)\n" + NA(response.number2);
        
        var maxsize= 256;
        sendAndStore({
            "heading":heading.substring(0,maxsize),
            "daystats":daystats.substring(0,maxsize),
            "weekstats":weekstats.substring(0,maxsize),
            "datechange":datechange,
        });
               
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
                          console.log("message recieved");                               
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