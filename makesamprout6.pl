#!/usr/bin/perl -w

$tot_cycs = 65;
$overhead = 28;
$tab = "\t";
$c030 = '$c030';
#$e0c030 = '$e0c030';
$eff_i = 0;
for($hi = 0; $hi < 2; $hi++) {
	$hi_str = "lo";
	if($hi) {
		$hi_str = "hi";
	}
	if($hi == 0) {
		print $tab . "ds	\\	; page align\n";
	}
	print "table" . $hi_str . "\n";
	for($i = 0; $i < 0x100; $i += 4) {
		$eff_i = int($i * $tot_cycs / 0xfc);
		print $tab . "jmp	samp" . $eff_i . $hi_str . 
				"	; " . $i . "\n";
		print $tab . "nop\n";
	}
}

for($hi = 0; $hi < 2; $hi++) {
	$hi_str = "lo";
	$swap_str = "hi";
	if($hi) {
		$hi_str = "hi";
		$swap_str = "lo";
	}
	for($i = 0; $i < ($tot_cycs + 1); $i++) {
		print "; Working on samp" . $i . $hi_str . "\n";
		print "samp" . $i . $hi_str . "\n";
		$eff_i = $i;
		if($hi) {
			$eff_i = $tot_cycs - $i;
		}
		if($eff_i == 0) {
			sub_jmp($hi_str, $tot_cycs - $overhead - 3);
			next;
		}
		if($eff_i <= 3) {
			print $tab . "ldx	#0	; 2\n";
			$cycs = $tot_cycs - $overhead - 5;
			for($j = 0; $j < $eff_i; $j++) {
				print $tab . "sta	$c030,x	; 5, " .
						"toggled one clk\n";
				$cycs -= 5;
			}
			&sub_jmp($hi_str, $cycs);
			next;
		}
		if($eff_i < ($tot_cycs - $overhead - 6 - 3 - 1)) {
			&sub_delay_4_27($hi_str, $eff_i);
			next;
		}
		if($eff_i >= ($overhead + 6 + 3 + 2)) {
			&sub_delay_38_64($swap_str, $eff_i);
			next;
		}
		&sub_delay_27_37($hi_str, $eff_i);
	}
}

exit(0);

sub
sub_jmp
{
	my ($histr, $skip) = @_;

	print $tab . "jmp	nextsamp" . $histr . "+1-" . $skip . $tab .
			"; 3, Kill " . $skip . " cycles\n";
	if($skip < 2) {
		die "sub_jmp of $skip is illegal\n";
	}
}

sub
sub_do_delay
{
	my ($del) = @_;

	if(($del < 0) || ($del == 1)) {
		die "Cannot do delay: $del\n";
	}
	while(($del >= 7) && ($del != 8)) {
		print $tab . "php		; 3. delay:" . $del . "\n";
		print $tab . "plp		; 4\n";
		$del -= 7;
	}
	if($del & 1) {
		print $tab . "stx	zp4	; 3. delay:". $del . "\n";
		$del -= 3;
	}
	while($del >= 1) {
		print $tab . "nop		; 2. delay:" . $del . "\n";
		$del -= 2;
	}
}

sub
sub_delay_4_27
{
	my ($histr, $pos) = @_;

	my $cyc = 28 - $pos;
	print $tab . "ldx	#0	; 2\n";
	my $delay = $pos - 4;
	if($pos == 5) {
		print $tab . "sta	$c030,x	; 5, " .  "toggled one clk\n";
		$cyc -= 4;
		$delay--;
	}
	print $tab . "sta	$c030 	; 4\n";
	sub_do_delay($delay);
	print $tab . "sta	$c030	; 4\n";
	&sub_jmp($histr, $cyc);
}

sub
sub_delay_38_64
{
	my ($histr, $pos) = @_;

	my $cyc = $pos - 3 - 4 - 2 - $overhead;
	my $delay = $tot_cycs - $pos + 2;
	sub_do_delay($delay);
	print $tab . "sta	$c030	; 4\n";
	&sub_jmp($histr, $cyc);
}

sub
sub_delay_27_37
{
	my ($histr, $pos) = @_;

	print $tab . "nop		; 2\n";
	print $tab . "sta	$c030	; 4\n";
	print $tab . "lda	(zp1),y	; 5\n";
	print $tab . 'and	#$fc	; 2' . "\n";
	print $tab . "sta	nextpatch" . $histr . "+1	; 4\n";
	print $tab . "pha		; 3\n";
	print $tab . "pla		; 4\n";
	print $tab . "iny		; 2\n";

	my $delay = $pos - 20 - 4;

	sub_do_delay($delay);

	print $tab . "sta	$c030	; 4\n";

	$delay = $tot_cycs - $pos - 15 - 3 - 6;
	sub_do_delay($delay);

	print $tab . "jmp	nextsamp" . $histr . "2	; 3\n";
}

