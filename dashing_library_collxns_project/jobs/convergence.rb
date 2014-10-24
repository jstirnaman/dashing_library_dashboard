# Populate a graph
 include Charting
 SCHEDULER.every '10s' do
  points = []
  # Convert some JSON back to a Ruby array of hashes
  IO.foreach("data/sql_items_removed_from_bldg.json", "\n") do |line|
     points = (JSON.parse line).to_time_chart("REMOVED", "ITEMS")
  end

  send_event('convergence', points: points)
end
