$: << '../../drivers/ruby2/lib'
require 'pp'
require 'rethinkdb'
extend RethinkDB::Shortcuts

JSPORT = ARGV[0]
CPPPORT = ARGV[1]

def eq_test(one, two)

  case one.class
  when Array
    return false if one.class != two.class
    return false if one.length != two.length
    return one.zip(two).map{ |a, b| eq_test(a, b) }.all?

  when Hash
    return false if one.class != two.class
    return false if a.keys.sort != b.keys.sort
    return a.keys.map{|k| eq_test(a[k], b[k])}.all?

  else
    if not [Fixnum, Float].member? one.class
      return false if one.class != two.class
    end
    return one == two
  end
end

def show x
  return (PP.pp x, "").chomp
end

def eq exp
  proc { |val|
    if ! eq_test(val, exp)
      puts "Equality comparison failed"
      puts "Value: #{show val}, Expected: #{show exp}"
      return false
    else
      return true
    end
  }
end

def eval_env; binding; end
$defines = eval_env

$js_conn = RethinkDB::Connection.new('localhost', JSPORT)

$cpp_conn = RethinkDB::Connection.new('localhost', CPPPORT)


def test src, expected, name
  begin
    query = eval src, $defines
  rescue Exception => e
    puts "#{name}: Error: #{e} in construction of #{src}"
    return
  end

  #TODO: uncomment when it works
  #print "#{name} JS: "
  #do_test query, expected, 'JS', $js_conn
  print "#{name} CPP: "
  do_test query, expected, 'CPP', $cpp_conn
end

def do_test query, expected, server, con    
  begin
    # TODO: query.run(con)
    res = query.run
  rescue Exception => e
    puts "Error running query: #{e}"
    return false
    end
  
  exp_fun = eval expected, $defines
  
  if ! exp_fun.kind_of? Proc
    exp_fun = eq exp_fun
  end
  
  if ! exp_fun.call res
    return false
  end
  
  puts "Success"
  return true
end

def define expr
  eval expr, $defines
end

def bag list
  bag = list.sort
  proc do |other|
    eq_test(bag, other.sort)
  end
end

True=true
False=false
