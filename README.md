this is the beginning of the juce version of gary4live

original frontend here https://github.com/betweentwomidnights/gary4live

the backend repo is here:

https://github.com/betweentwomidnights/gary-backend-combined

a note about the backend... as i dove into this i realized websockets are a hellscape in c++

our backend will now support http requests for every route along with the websockets it already does.

features implemented so far:

backend connection (just a health check)
auto-record and save input buffer from any DAW

the rest should be pretty easy now.