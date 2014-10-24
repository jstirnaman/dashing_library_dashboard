# buzzwords = ['Paradigm shift', 'Leverage', 'Pivoting', 'Turn-key', 'Streamlininess', 'Exit strategy', 'Synergy', 'Enterprise', 'Web 2.0'] 
# buzzword_counts = Hash.new({ value: 0 })

SCHEDULER.every '5s' do
  location_counts = Hash.new
  # Convert some JSON back to a Ruby array of hashes
  IO.foreach("data/sql_items_in_building.json") do |line|
    (JSON.parse line).first(10).each {|h| location_counts[h["x"]] = { label: h["x"].sub("Dykes", '').strip, value: h["y"] } }
  end
#   location_counts[random_buzzword] = { label: random_buzzword, value: (buzzword_counts[random_buzzword][:value] + 1) % 30 }
  
  send_event('buzzwords', { items: location_counts.values })
end
