#!mruby

v8 = V8.new
puts v8.eval('1+2')

v8.add_func("plus") do |lhs,rhs|
  lhs + rhs
end

puts v8.eval("plus(2,3)")
