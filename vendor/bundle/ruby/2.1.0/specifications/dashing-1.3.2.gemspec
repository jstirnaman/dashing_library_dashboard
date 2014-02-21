# -*- encoding: utf-8 -*-
# stub: dashing 1.3.2 ruby lib

Gem::Specification.new do |s|
  s.name = "dashing"
  s.version = "1.3.2"

  s.required_rubygems_version = Gem::Requirement.new(">= 0") if s.respond_to? :required_rubygems_version=
  s.require_paths = ["lib"]
  s.authors = ["Daniel Beauchamp"]
  s.date = "2013-11-21"
  s.description = "This framework lets you build & easily layout dashboards with your own custom widgets. Use it to make a status boards for your ops team, or use it to track signups, conversion rates, or whatever else metrics you'd like to see in one spot. Included with the framework are ready-made widgets for you to use or customize. All of this code was extracted out of a project at Shopify that displays dashboards on TVs around the office."
  s.email = "daniel.beauchamp@shopify.com"
  s.executables = ["dashing"]
  s.files = ["bin/dashing"]
  s.homepage = "http://shopify.github.com/dashing"
  s.rubygems_version = "2.2.2"
  s.summary = "The exceptionally handsome dashboard framework."

  s.installed_by_version = "2.2.2" if s.respond_to? :installed_by_version

  if s.respond_to? :specification_version then
    s.specification_version = 4

    if Gem::Version.new(Gem::VERSION) >= Gem::Version.new('1.2.0') then
      s.add_runtime_dependency(%q<sass>, [">= 0"])
      s.add_runtime_dependency(%q<coffee-script>, [">= 1.6.2"])
      s.add_runtime_dependency(%q<execjs>, [">= 2.0.0"])
      s.add_runtime_dependency(%q<sinatra>, [">= 0"])
      s.add_runtime_dependency(%q<sinatra-contrib>, [">= 0"])
      s.add_runtime_dependency(%q<thin>, [">= 0"])
      s.add_runtime_dependency(%q<rufus-scheduler>, ["~> 2.0"])
      s.add_runtime_dependency(%q<thor>, [">= 0"])
      s.add_runtime_dependency(%q<sprockets>, [">= 0"])
      s.add_runtime_dependency(%q<rack>, [">= 0"])
    else
      s.add_dependency(%q<sass>, [">= 0"])
      s.add_dependency(%q<coffee-script>, [">= 1.6.2"])
      s.add_dependency(%q<execjs>, [">= 2.0.0"])
      s.add_dependency(%q<sinatra>, [">= 0"])
      s.add_dependency(%q<sinatra-contrib>, [">= 0"])
      s.add_dependency(%q<thin>, [">= 0"])
      s.add_dependency(%q<rufus-scheduler>, ["~> 2.0"])
      s.add_dependency(%q<thor>, [">= 0"])
      s.add_dependency(%q<sprockets>, [">= 0"])
      s.add_dependency(%q<rack>, [">= 0"])
    end
  else
    s.add_dependency(%q<sass>, [">= 0"])
    s.add_dependency(%q<coffee-script>, [">= 1.6.2"])
    s.add_dependency(%q<execjs>, [">= 2.0.0"])
    s.add_dependency(%q<sinatra>, [">= 0"])
    s.add_dependency(%q<sinatra-contrib>, [">= 0"])
    s.add_dependency(%q<thin>, [">= 0"])
    s.add_dependency(%q<rufus-scheduler>, ["~> 2.0"])
    s.add_dependency(%q<thor>, [">= 0"])
    s.add_dependency(%q<sprockets>, [">= 0"])
    s.add_dependency(%q<rack>, [">= 0"])
  end
end
