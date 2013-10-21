# Gems required to run web app
require 'sinatra'
require 'sinatra/json'
require 'redis'
require 'json'

# Connection to the local Redis server
r = Redis.new

# Display main index page
get '/' do  
  send_file File.join(settings.public_folder, 'index.html') 
end 

# Page to accept HTTP GET requests from the JavaScript, poll every second for info
get '/get' do
	@host = r.get("host") # Need Arduino IP for ping
	@temp = r.get("plot") 
	@error = r.get("error")
	@src = r.get("source")
	@ping = %x[ping -c 1 -n -W 5 #{@host} | grep -Ec "100[.]*[0]*% packet loss"] # check if Arduino is online (our version of connected)
	if @ping.inspect == '"1\n"' # if ping fails, send specific info
		@hash = { :temp => "N", :err => "Unplugged" ,:src => "#{@src}",:ping => "#{@ping.gsub("\n", "")}",:host => "#{@host}" }
	else
		@hash = { :temp => "#{@temp}", :err => "#{@error}" ,:src => "#{@src}",:ping => "#{@ping.gsub("\n", "")}",:host => "#{@host}" }
	end
	@json = @hash.to_json
	"#{@json}" 
end

# Page to accept HTTP GET requests from the Arduino, Turn LEDs on or off
get '/display' do
	@display = r.get("display")
	"#{@display}"
end

# Page to accept HTTP POST requests from the Arduino
post '/json' do
 	content_type :json
	@data = JSON.parse(request.body.read)
	@data.each_pair do |key, val|
		r.set(key, val)
	end
	r.set("host", request.ip)
	redirect to ('/')
end
