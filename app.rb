require 'sinatra'
require 'sinatra/json'
require 'redis'
require 'json'

r = Redis.new

get '/' do  
  send_file File.join(settings.public_folder, 'index.html') 
end 

get '/get' do
	@host = r.get("host")
	@temp = r.get("plot")
	@error = r.get("error")
	@src = r.get("source")
	@ping = %x[ping -c 1 -n -W 5 #{@host} | grep -Ec "100[.]*[0]*% packet loss"]
	if @ping.inspect == '"1\n"'
		@hash = { :temp => "N", :err => "Unplugged" ,:src => "#{@src}",:ping => "#{@ping.gsub("\n", "")}",:host => "#{@host}" }
	else
		@hash = { :temp => "#{@temp}", :err => "#{@error}" ,:src => "#{@src}",:ping => "#{@ping.gsub("\n", "")}",:host => "#{@host}" }
	end
	@json = @hash.to_json
	"#{@json}" 
end

get '/display' do
	@display = r.get("display")
	"#{@display}"
end

post '/json' do
 	content_type :json
	@data = JSON.parse(request.body.read)
	@data.each_pair do |key, val|
		r.set(key, val)
	end
	r.set("host", request.ip)
	redirect to ('/')
end
