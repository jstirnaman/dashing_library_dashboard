# buzzwords = ['Paradigm shift', 'Leverage', 'Pivoting', 'Turn-key', 'Streamlininess', 'Exit strategy', 'Synergy', 'Enterprise', 'Web 2.0'] 
# buzzword_counts = Hash.new({ value: 0 })

SCHEDULER.every '5s' do
  location_counts = Hash.new
  # Convert some JSON back to a Ruby array of hashes
  IO.foreach("data/sql_items_in_bldg.json", "\n") do |line|
    (JSON.parse line).to_chart("LOCATION", "ITEMS").first(10).each {|h| location_counts[h[:x]] = { label: h[:x].sub("Dykes", '').strip, value: h[:y] } }
  end
  
  send_event('buzzwords', { items: location_counts.values })
end
