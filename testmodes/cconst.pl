#!/usr/bin/perl

use warnings;

print "Verilog -> C constants converter for testbench generation tools\n";

my ($in, $out);
my $set = ""; # marks a "define" like -D<DEFINE>

SWITCH: {
	@ARGV == 0 && do { 
		$in  = "../spikenet_base.vh"; # default input/output file
		$out = $in;
		last SWITCH;
	};
	@ARGV == 1 && do { 
		$in  = $ARGV[0];
		$out = $in;
		$out =~ s/v//;     # .vh => .h
		$out =~ s/^\.\.\///; # remove ../ in front :)
		last SWITCH;
	};
	@ARGV == 2 && do {
		$in  = $ARGV[0];
		$out = $ARGV[1];
		last SWITCH;
	};
	@ARGV == 3 && do {
		$in  = $ARGV[0];
		$out = $ARGV[1];
		$set = $ARGV[2];
		last SWITCH;
	};
}

open(BASE, $in) or die "Can't open $in";
open(NEW, ">$out") or die "Can't open $out";

my $sum = 0;
my $within_ifdef = "";
while($line=<BASE>) {
	my($def,$name,$val,$rest);
	($def,$name,$val,$rest)=split(" ",$line);
	next if (not defined $def);
	if($def =~ /`/ ){

		if($def eq "`ifdef") {
			$line =~ m/`ifdef ([\w\d]+)/;
			$within_ifdef = $1;
		} elsif ($def eq "`endif") {
			$within_ifdef = "";
		}
		
		if($def eq "`define"){
			#some verilog cpp conversion
			#remove ticks
			$val =~ s/[`']//g;
			#preserve comments at end of line
			if($line =~ /\/\//){
				$rest = $& . $'; 
			} else { $rest="\n"; }
			if (($set eq "") or ($within_ifdef eq "") or ($within_ifdef eq $set)) {
				$sum=$sum+1;
				print NEW ("#define $name $val $rest");
			}
		}
	}else {print NEW $line;}
}

print "Created $out with $sum constants\n";
