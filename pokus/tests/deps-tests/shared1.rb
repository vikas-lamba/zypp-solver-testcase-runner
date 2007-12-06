#!/usr/bin/ruby


# Test sharing tags.


require 'zypptools/lib/package'
require 'zypptools/output/output'
include ZYppTools

require '../../src/testsuite'


packages = Array.new()

pkg = Package.new("black-cat")
pkg.arch = "i586"
pkg.version = "1.2"
pkg.release = 1
pkg.summary = "Comet 460"
packages.push(pkg)

pkg = Package.new("black-cat")
pkg.arch = "i686"
pkg.version = "1.2"
pkg.release = 1
pkg.shared = "black-cat 1.2 1 i586"
packages.push(pkg)


path = Testsuite::write_repo(:yast, packages)


Testsuite::set_arch("i686")
pool = Testsuite::read_repo("file://" + path)

Testsuite::haha2(pool).each do |res|
    puts "#{res.kind} #{res.name} #{res.edition.to_s} #{res.arch.to_s}"
    Testsuite::dump_deps(res, Dep.PROVIDES)
    puts "  Summary: #{res.summary}"
    puts
end
