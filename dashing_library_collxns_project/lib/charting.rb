require 'bigdecimal'
module Charting
# Helper methods for charting.
def to_time_chart(x = nil, y = nil)
# If keys x and y are passed then format those values for charting.
# If no x and y, then use the first two values of each hash.
# Assumes your data is already in the order you need.
  data = self.map do |h| 
           unless h.nil?
             points = {}
             if !x.nil? && !y.nil?
               points[:x] = h[x] ? try_convert_to_rickshaw(h[x]).to_i : "0"
               points[:y] = h[y] ? try_convert_to_rickshaw(h[y]).to_i : "0"
             else
               a = h.to_a
               x = a[0][1]
               points[:x] = x ? try_convert_to_rickshaw(x).to_i : "0"
               y = a[1][1]
               points[:y] = y ? try_convert_to_rickshaw(y).to_i : "0"
             end
             points
           end
  end

  # Example for using Net::HTTP to send encoded json data to Dashing's Sample Convergence graph widget:
  # widget_uri = URI('http://0.0.0.0:3030/widgets/convergence')
  # req = Net::HTTP::Post.new widget_uri.path
  # res = Net::HTTP.start(widget_uri.host, widget_uri.port, :use_ssl => false) {|http| http.request req}
  # req.body = {:auth_token => 'YOUR_AUTH_TOKEN', :points => [{:x=>1383022800, :y=>51}, {:x=>1383109200, :y=>34}, {:x=>1383195600, :y=>20}]}.to_json
end

		def to_chart
			data = self.query_result.map do |h| 
							 unless h.nil?
								 points = {}
								 a = h.to_a
								 x = a[0][1]
								 points[:x] = x ? x : ""
								 y = a[1][1]
								 points[:y] = y ? try_convert_to_rickshaw(y).to_i : "0"
								 points
							 end
			end
		end
		
		def try_convert_to_rickshaw(value)
			# Dashing uses Rickshaw.js for graphing time series.
			# Rickshaw expects time values in epoch seconds.
			if value.class == BigDecimal
				value = value
			elsif value.class == Time
				value = value.strftime('%s')
			elsif value.class == String
				dateparts = value.match(/(\d{4}).?(\d{2}).?(\d{2})/)
				if !dateparts.nil?
					# We have datetime values converted to a string.
					date = Time.new(dateparts[1], dateparts[2], dateparts[3])
					value = date.strftime('%s')
				elsif !value.match(/^\d.*E\d.*/).nil?
				  # We have BigDecimal values converted to a string.
				  value = BigDecimal.new(value)
				end
			end
			value 
		end	

end