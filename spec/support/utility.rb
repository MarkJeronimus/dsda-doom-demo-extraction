module Utility
  extend self
  
  def play_demo(lmp:, iwad: "DOOM2.WAD", pwad: nil)
    command = "./build/prboom-plus.exe -iwad spec/support/wads/#{iwad}"
    command << " -file spec/support/wads/#{pwad}" if pwad
    command << " -fastdemo spec/support/lmps/#{lmp}"
    command << " -nosound -nomusic -nodraw -levelstat -analysis"
    
    system(command)
  end
  
  def read_analysis
    Analysis.new
  end
  
  def read_levelstat(filename = "levelstat.txt")
    Levelstat.new(filename)
  end
  
  class Analysis
    def initialize
      @data = Hash[File.readlines("analysis.txt", chomp: true).map(&:split)]
    end
    
    def pacifist?
      @data['pacifist'] == '1'
    end
    
    def reality?
      @data['reality'] == '1'
    end
    
    def almost_reality?
      @data['almost_reality'] == '1'
    end
    
    def hundred_k?
      @data['100k'] == '1'
    end
    
    def missed_monsters
      @data['missed_monsters'].to_i
    end
    
    def missed_secrets
      @data['missed_secrets'].to_i
    end
    
    def tyson_weapons?
      @data['tyson_weapons'] == '1'
    end
  end
  
  class Levelstat
    def initialize(filename)
      @data = File.readlines(filename, chomp: true).map(&:split)
    end
    
    def total
      return '00:00' unless @data.last
      
      @data.last[3].gsub(/[()]/, '')
    end
  end
end
