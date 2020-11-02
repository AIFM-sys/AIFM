#!/bin/bash

#########################################################################################
##Software Implementation, Non-parametric Hierarchical Performance Testing (HPT)
##Authors: Yue Wu (Email: wuyue@ict.ac.cn) and Tianshi Chen
##Institute of Computing Technology, Chinese Academy of SCiences
##Beijing 100190, China
##Last Update: 17th June, 2013
##HPT on the web: http://novel.ict.ac.cn/tchen/hpt/
##
##Reference: T. Chen, Y. Chen, Q. Guo, O. Temam, Y. Wu, and W. Hu,  
##"Statistical Performance Comparisons of Computers", 
##In Proceedings of HPCA-18, New Orleans, Louisiana, 2012.
##
##Provided for non-commercial research and educational use only. 
##Not for reproduction or distribution or commercial use. 
##
##
##Please retain the above information when you reuse the code in your own tool.
#########################################################################################

configfile=$1
if [ -z "${configfile}" ]; then
    configfile="parameter.cfg"
fi 
ACC=20		# number of digits after the decimal point
acc_maxsu=2
NCOL_BASELINE=5

select1_table_file=table_select1
select2_table_file=table_select2


#########################################################################
#								Math Part
#########################################################################

display_matrix()
{
	# FUNCTION: display a data table
	# FORMAT: display_matrix nrow ncol MTRX name
	# OUTPUT: (None)
	local i; local j; local mtrx
	eval mtrx=(\${$3[@]})
	printf "\n========================================\n"
	printf "Data matrix($4):\n"
	for i in `seq 0 $(($1-1))`; do
		for j in `seq 0 $(($2-1))`; do
			printf "%12.3f " ${mtrx[$(($i*$2+$j))]} 
		done
		printf "\n"
	done
}

combination()
{
	# FUNCTION: return "n choose k", for example, `combination 4 2` equals to 6
	# FORMAT: combination n m
	# OUTPUT: `combination n m`
	local A=$1
	local B=1
	local RES=1
	local i
	for i in `seq 1 $2`
	do
		RES=$(($RES*$A/$B))
		A=$(($A-1))
		B=$(($B+1))
	done
	expr $RES
}

cdfnorm()
{
	# FUNCTION: an approximation of cumulative density function for standard norm distribution
	# FORMAT: cdfnorm x
	# OUTPUT: ret_cdf
	local ACC=24
	local L=$1
	local a1=0.31938153; local a2=-0.356563782; local a3=1.781477937; local a4=-1.821255978
	local a5=1.330274429; local Pi=3.141592653589793238462643

	if [ `echo "$L<0"|bc` -eq 1 ]; then
		L=`echo "scale=$ACC;(-1)*$L"|bc`
	fi
	local K=`echo "scale=$ACC;1/(1+0.2316419*$L)"|bc`
	local tmp=`echo "(((($a5*$K+$a4)*$K+$a3)*$K+$a2)*$K+$a1)*$K"|bc`
	tmp=`echo "scale=$ACC;e(0-$L*$L/2)*$tmp/sqrt(2*$Pi)"|bc -l`
	if [[ `echo "$1>0"|bc` -eq 1 ]]; then
		tmp=`echo "scale=$ACC;1-$tmp"|bc`
	fi
	ret_cdf=`printf "%.${ACC}f" $tmp`
}

qnorm()
{
	# FUNCTION: quantile function of standard norm distribution
	# FORMAT: qnorm p
	# OUTPUT: ret_qnorm
	local p=$1
	if [[ `echo "$p<=0"|bc` -eq 1 ]] || [[ `echo "$p>=1"|bc` -eq 1 ]]; then
		echo "[Error][in function \"qnorm\"] the value of p is out of range."
		unset ret_qnorm
		return
	fi
	if [[ `echo "$p==0.5"|bc` -eq 1 ]]; then
		ret_qnorm=0
	else
		y=`echo "scale=$ACC;-l(4*$p*(1-$p))"|bc -l`
		b=(1.570796288 0.03706987906 -0.0008364353589 -0.0002250947176 0.000006841218299 0.000005824238515 -0.00000104527497 0.00000008360937017 -0.000000003231081277 0.00000000003657763036 0.0000000000006936233982)
		u=0
		pow=1
		for i in `seq 0 $((${#b[@]}-1))`; do
			pow=`echo "scale=$ACC;$pow*$y"|bc`
			u=`echo "scale=$ACC;$u+$pow*${b[$i]}"|bc`
		done
		u=`echo "scale=$ACC;sqrt($u)"|bc`
		if [[ `echo "$p<0.5"|bc` -eq 1 ]]; then
			u=`echo "scale=$ACC;$u*(-1)"|bc`
		fi
		ret_qnorm=$u
	fi
}


get_rank()
{
	# FUNCTION: get rank and repeating times of a sequence
	#			for example, seq_x=(22 33 11 22), then seq_rank=(2 4 1 2), seq_rep=(2 1 1 2)
	# FORMAT: get_rank len seq_x seq_rank seq_rep
	# OUTPUT: (seq_rank, seq_rep)
	local i; local j; local tmp0; local tmp1; local tmp2; local cmpr; local gr_x
	local len=$1
	eval gr_x=(\${$2[@]})
	
#####  CAUTION  #######
	unset "$3"		###
	unset "$4"		###
#####  CAUTION  #######

	for i in `seq 0 $(($len-1))`; do
		tmp0=${gr_x[$i]}
		tmp1=0
		tmp2=0
		for j in `seq 0 $(($len-1))`; do
			cmpr=${gr_x[$j]}
			if [[ `echo "$cmpr<$tmp0"|bc` -eq 1 ]]; then
				tmp1=$(($tmp1+1))
			elif [[ `echo "$cmpr==$tmp0"|bc` -eq 1 ]]; then
				tmp2=$(($tmp2+1))
			fi
		done
		eval $3[$i]=$(($tmp1+1))
		eval $4[$i]=$tmp2
	done
}

get_ranksum()
{
	# FUNCTION: get the rank-sum of a subsequence
	#			for example, if start=0, end=1, seq_rank=(2 4 1 2), rep=(2 1 1 2), then ret_ranksum=2.5+4=6.5
	# FORMAT: get_ranksum start end seq_rank seq_rep
	# OUTPUT: $ret_ranksum
	local i; local rk; local n; local grs_rank; local grs_rep
	unset grs_rank; unset grs_rep
	eval grs_rank=(\${$3[@]})
	eval grs_rep=(\${$4[@]})
	local w=0
	for i in `seq $1 $2`; do
		eval rk=${grs_rank[$i]}
		eval n=${grs_rep[$i]}
		w=`echo "scale=1;$w+$rk+($n-1)/2"|bc`		## scale=1 is enough
	done
	ret_ranksum=$w
}


thexth()
{
	# FUNCTION: return the "x"th smallest number in the sequence {x_0 x_1 x_2 ... x_(len-1)}
	#			the idea of "quick sort" is used in this implementation
	# FORMAT: thexth len xth x_0 x_1 x_2 ... x_(len-1)
	# OUTPUT: ret_thexth
	local i; local tmp
	local arr; local arr0; local arrl; local arrg
	local len=$1
	local xth=$2
	local il=0
	local ig=0

	eval arr0=(\$@)
	for i in `seq 0 $(($len-1))`; do
		tmp=$(($i+2))
		eval arr[$i]=${arr0[$tmp]}
	done
	local ref=${arr[0]}

	for i in `seq 1 $(($len-1))`; do
		tmp=${arr[$i]}
		if [[ `echo "$tmp<$ref"|bc` -eq 1 ]]; then
			arrl[$il]=$tmp
			il=$(($il+1))
		elif [[ `echo "$tmp>$ref"|bc` -eq 1 ]]; then
			arrg[$ig]=$tmp
			ig=$(($ig+1))
		fi
	done
	if [[ $(($len-$ig)) -lt $xth ]]; then
		thexth $ig $((xth-$len+$ig)) ${arrg[@]}
	elif [[ $il -lt $xth ]]; then
		ret_thexth=$ref
	else
		thexth $il $xth ${arrl[@]}
	fi
}

get_median()
{
	# FUNCTION: return the median of the subsequence of seq_x, from (start)th element to (end)th element
	# FORMAT: get_median start end seq_x
	# OUTPUT: ret_median
	local tmp; local arr0; local arr; local i
	local start=$1
	local end=$2
	eval arr0=(\${$3[@]})
	for i in `seq $start $end`; do
		tmp=$(($i-$start))
		arr[$tmp]=${arr0[$i]}
	done
	local len=$(($end-$start+1))
	if [[ $(($len%2)) -eq 1 ]]; then
		tmp=$(($len/2+1))
		thexth $len $tmp ${arr[@]}
		ret_median=$ret_thexth
	else
		tmp=$(($len/2))
		thexth $len $tmp ${arr[@]}
		ret_median=$ret_thexth
		tmp=$(($len/2+1))
		thexth $len $tmp ${arr[@]}
		ret_median=`echo "scale=$ACC;($ret_thexth+$ret_median)/2"|bc`
	fi
}


prepare_one_row()
{
	# FUNCTION: the length of seq_x is col1+col2. define seq_l as the subsequence containing the first col1 elements 
	#			of seq_x, and seq_r as the subsequence containing the last col2 elements of seq_x. Return the median 
	#			of seq_l and seq_r seperately(ml & mr), and the ranksum of the left part and the right part(wl & wr).
	# FORMAT: prepare_one_row col1 col2 seq_x
	# OUTPUT: ml,mr,wl,wr
	local por_x; local por_rank; local por_rep
	unset por_x; unset por_rank; unset por_rep
	eval por_x=(\${$3[@]})
	get_rank $(($1+$2)) por_x por_rank por_rep		## get_rank has sentences "unset rank; unset rep"
	get_ranksum 0 $(($1-1)) por_rank por_rep
	wl=$ret_ranksum
	get_ranksum $1 $(($1+$2-1)) por_rank por_rep
	wr=$ret_ranksum
	get_median 0 $(($1-1)) por_x
	ml=$ret_median
	get_median $1 $(($1+$2-1)) por_x
	mr=$ret_median
}


select1()
{
	# FUNCTION: return the number of subsets of set {1,2,...,n}, the sum of elements of which is no greater than target
	# FORMAT: select1 n target
	# OUTPUT: ret_select1

	local n=$1
	local target=$2
	local tag=0
	local itg; local tmp; local tmp0; local HEAD; local CONTENT

	if [[ $n -lt 25 ]]; then
		itg=`echo "scale=2;$target+1"|bc`
		itg=${itg%.*}
		itg=$(($itg-1))
		while read HEAD CONTENT; do
			if [ "$HEAD" == [$n] ]; then
				tag=1;
				break;
			fi
		done <$select1_table_file
		if [[ $tag -eq 1 ]]; then
			CONTENT=`echo $CONTENT`
			CONTENT=" $CONTENT"
			if [[ `echo "$itg==$target"|bc` -eq 1 ]]; then
				tmp=${CONTENT##* $itg:}
				tmp0=${tmp%% *}
				itg=$(($itg-1))
				if [[ $itg -lt 0 ]]; then
					tmp=0
				else
					tmp=${CONTENT##* $itg:}
					tmp=${tmp0%% *}
				fi
				tmp=`echo "scale=2;$tmp0/2+$tmp/2"|bc`
			else
				tmp=${CONTENT##* $itg:}
				tmp=${tmp%% *}
			fi
		else
			echo "[Error][in function \"select1\"] Cannot find the entry."
			tmp=-1
		fi
	fi
	ret_select1=$tmp
}



ranksum_table()
{
	# FUNCTION: calculate the upper and lower bound of rank-sum in a rank-sum test of a sequence with length "n" 
	#			under the significant level "alpha".
	# FORMAT: ranksum_table n alpha
	# OUTPUT: ret_rst_n ret_rst_alpha ret_rst_upper ret_rst_lower

	local tag=0; local HEAD; local CONTENT; local pos; local value; local res; local tmp; local bound
	local n=$1
	local alpha=$2
	local mu; local stddev

	if [[ `echo "$alpha<=0"|bc` -eq 1 ]] || [[ `echo "$alpha>=0.5"|bc` -eq 1 ]]; then
		echo "[Error][in function \"ranksum_table\"] Alpha value($alpha) is out of range"
	fi

	if [[ $n -eq $ret_rst_n ]] && [[ `echo "$alpha==$ret_rst_alpha"|bc` -eq 1 ]]; then
		return
	fi

	if [[ $n -le 12 ]] && [[ $n -ge 3 ]]; then
		tag=0
		while read HEAD CONTENT; do
			if [ "$HEAD" == [$n] ]; then
				tag=1;
				break;
			fi
		done <$select2_table_file

		if [[ tag -eq 0 ]]; then
			echo "[Error][in function \"ranksum_table\"] Cannot find the entry."
			ret_rst_upper=-1; ret_rst_lower=-1; ret_rst_n=-1; ret_rst_alpha=-1
			return
		fi
		bound=`combination $(($n*2)) $n`
		bound=`echo "scale=$ACC;$bound*$alpha"|bc`
		res=$CONTENT
		while [[ 1 -eq 1 ]]; do
			pos=${res%%:*}
			res=${res#*:}
			value=${res%% *}
			res=${res#* }
			if [[ `echo "$value<=$bound"|bc` -eq 1 ]]; then
				tmppos=$pos
				tmpvalue=$value
			else
				break
			fi
		done
		if [[ `echo "$value+$tmpvalue<=2*$bound"|bc` -eq 1 ]]; then
			ret_rst_lower=$pos
			ret_rst_upper=$(($n*($n*2+1)-$ret_rst_lower))
		else
			ret_rst_lower=`echo "scale=2;$tmppos+0.5"|bc`
			ret_rst_upper=`echo "scale=2;$n*($n*2+1)-$ret_rst_lower"|bc`
		fi
		ret_rst_n=$n
		ret_rst_alpha=$alpha
	elif [[ $n -gt 12 ]]; then
		qnorm $alpha
		mu=`echo "scale=$ACC;$n*($n*2+1)/2"|bc`
		stddev=`echo "scale=$ACC;sqrt((2*$n+1)/3)*$n/2"|bc`
		tmp=`echo "scale=$ACC;$ret_qnorm*$stddev"|bc`
		ret_rst_lower=`echo "scale=$ACC;$mu-($tmp)"|bc`
		ret_rst_upper=`echo "scale=$ACC;$mu+($tmp)"|bc`
		ret_rst_n=$n
		ret_rst_alpha=$alpha
	fi

}


unibench()
{
	# FUNCTION: the uni-bench level test for the sequence "seq_x" with length "n" under the significant level "alpha"
	#			if significant, return diff_median(=median_l-median_r); else return 0.
	# FORMAT: unibench n alpha seq_x
	# OUTPUT: ret_unibench
	local n=$1
	local alpha=$2
	local ub_x
	eval ub_x=(\${$3[@]})

	prepare_one_row $n $n ub_x
	local target=$wl

	ret_unibench=0
	if [[ $n -gt 2 ]]; then
		ranksum_table $n $alpha
		if [[ `echo "$target<=$ret_rst_lower"|bc` -eq 1 ]] || [[ `echo "$target>=$ret_rst_upper"|bc` -eq 1 ]]; then
			ret_unibench=`echo "$ml-$mr"|bc`
		fi
	elif [[ $n -le 2 ]]; then
		ret_unibench=`echo "$ml-$mr"|bc`
	fi
}



crossbench()
{
	# FUNCTION: the cross-bench level test. "seq_x" represents the diff_median sequence with length "row"
	#			this function will return the reliability of the hypothesis "data_a is better than data_b".
	# FORMAT: crossbench row seq_x
	# OUTPUT: ret_crossbench ret_cb_wp ret_cb_wn
	local row=$1
	local cb_x; local cb_rank; local cb_rep; local sign
	eval cb_x=(\${$2[@]})
	local i; local tmp1; local tmp2
	local wp=0
	local wn=0

	for i in `seq 0 $(($row-1))`; do
		if [[ `echo "${cb_x[$i]}<0"|bc` -eq 1 ]]; then
			sign[$i]=-1
			cb_x[$i]=`echo "scale=$ACC;${cb_x[$i]}*(-1)"|bc`
		elif [[ `echo "${cb_x[$i]}>0"|bc` -eq 1 ]]; then
			sign[$i]=1
		else
			sign[$i]=0
		fi
	done
	get_rank $row cb_x cb_rank cb_rep
	for i in `seq 0 $(($row-1))`; do
		if [[ ${sign[$i]} -eq 1 ]]; then
			wp=`echo "scale=$ACC;$wp+${cb_rank[$i]}+${cb_rep[$i]}/2-1/2"|bc`
		elif [[ ${sign[$i]} -eq -1 ]]; then
			wn=`echo "scale=$ACC;$wn+${cb_rank[$i]}+${cb_rep[$i]}/2-1/2"|bc`
		else
			wp=`echo "scale=$ACC;$wp+${cb_rank[$i]}/2+${cb_rep[$i]}/4-1/4"|bc`
			wn=`echo "scale=$ACC;$wn+${cb_rank[$i]}/2+${cb_rep[$i]}/4-1/4"|bc`
		fi
	done

	if [[ -n "$ret_cb_wp" ]] && [[ -n "$ret_cb_wn" ]]; then				## not NULL
		if [[ `echo "$ret_cb_wp==$wp"|bc` -eq 1 ]] && [[ `echo "$ret_cb_wn==$wn"|bc` -eq 1 ]]; then
			return		## skip re-calculating
		fi
	fi

	ret_cb_wp=$wp
	ret_cb_wn=$wn

	if [[ $row -lt 25 ]]; then
		select1 $row $wp
		tmp1=$ret_select1
		tmp2=$((2**$row))
		ret_crossbench=`echo "scale=$ACC;$tmp1/$tmp2"|bc`
	else
		tmp1=`echo "scale=$ACC;$wp-$row*($row+1)/4"|bc`
		tmp2=`echo "scale=$ACC;sqrt($row*($row+1)*(2*$row+1)/24)"|bc`
		tmp1=`echo "scale=$ACC;$tmp1/$tmp2"|bc`
		cdfnorm $tmp1
		ret_crossbench=$ret_cdf
	fi

}


hpt_basic()
{
	# FUNCTION: Do HPTest for the two data matrices(MTRX1' & MTRX2, MTRX1'=multi*MTRX1), using "alpha_uni" as the 
	#			significant level in the uni-bench level test. The size of both matrices is row*col
	# FORMAT: hpt_basic row col multi alpha_uni MTRX1 MTRX2
	# OUTPUT: ret_hpt_basic ret_hpt_wp ret_hpt_wn
	local hpt_x
	local seq_rank; local seq_rep; local MTRX1; local MTRX2; local meddiff
	local i; local j; local k; local tmp1; local tmp2
	local row=$1
	local col=$2
	local multi=$3
	local alpha_uni=$4
	eval MTRX1=(\${$5[@]})
	eval MTRX2=(\${$6[@]})

	for i in `seq 0 $(($row-1))`; do
		unset upt_x
		for j in `seq 0 $(($col-1))`; do
			hpt_x[$j]=`echo "scale=$ACC;$multi*${MTRX1[$(($i*$col+$j))]}"|bc`
			hpt_x[$(($j+$col))]=${MTRX2[$(($i*$col+$j))]}
		done
		unibench $col $alpha_uni hpt_x
		meddiff[$i]=$ret_unibench
	done

#	echo "hpt: meddiff(${#meddiff[@]}): ${meddiff[@]}"
	crossbench $row meddiff
	ret_hpt_basic=$ret_crossbench
	ret_hpt_wp=$ret_cb_wp
	ret_hpt_wn=$ret_cb_wn

#	echo ============  ret_hpt_basic=$ret_hpt_basic,wp=$ret_hpt_wp,wn=$ret_hpt_wn  =============

}


maxspeedup()
{
	# FUNCTION: return the maximum speedup value under reliability "reliability". Most of the parameters are the 
	#			same as the function hpt_basic.
	# FORMAT: maxspeedup row col reliability alpha_uni bool_wp_smaller MTRX1 MTRX2
	# OUTPUT: ret_maxspeedup
	local row=$1
	local col=$2
	local reliability=$3
	local alpha=$4
	local bool_wp_smaller=$5
	local MMTRX1; local MMTRX2
	eval MMTRX1=(\${$6[@]})
	eval MMTRX2=(\${$7[@]})
	local su; local basesu; local myscale; local min; local max; local mid; local step; local reci


	if [[ `echo "$reliability<0.5"|bc` -eq 1 ]]; then
		echo "[Warning: Skip Calculating][in function \"maxspeedup\"] The reliability value($reliability), which is less than 0.5, will lead to a meaningless conclusion. Skip calculating."
		ret_maxspeedup=-2
		return
	elif [[ $bool_wp_smaller -eq 1 ]]; then
		su=10
		hpt_basic $row $col $su $alpha MMTRX1 MMTRX2
		if [[ `echo "$ret_hpt_basic<1-$reliability"|bc` -eq 1 ]]; then
			echo "[Error][in function \"maxspeedup\"] Overflow: the maximum_speedup is beyond the upper bound 10. Stop calculating."
			ret_maxspeedup=-1
			return
		else
			step=-1
			myscale=1
			min=1
			max=10
			basesu=0
			while [[ $step -le $acc_maxsu ]]; do
				mid=$((($max-$min)/2+$min))
				su=`echo "scale=$ACC;$basesu+$myscale*$mid"|bc`
				hpt_basic $row $col $su $alpha MMTRX1 MMTRX2
				if [[ `echo "$ret_hpt_basic<1-$reliability"|bc` -eq 1 ]]; then
					min=$mid
				else
					max=$mid
				fi				

				if [[ $min -eq $(($max-1)) ]]; then
					basesu=`echo "scale=$ACC;$basesu+$min*$myscale"|bc`
					myscale=`echo "scale=$ACC;$myscale/10"|bc`
					step=$(($step+1))
					min=0
					max=10
				fi
			done
			ret_maxspeedup=`printf "%.${acc_maxsu}f" $basesu`
		fi
	else		#elif [[ $bool_wp_smaller -ne 1 ]]; then
		su=10
		reci=`echo "scale=$ACC;1/$su"|bc`
		hpt_basic $row $col $reci $alpha MMTRX1 MMTRX2
		if [[ `echo "$ret_hpt_basic>$reliability"|bc` -eq 1 ]]; then
			echo "[Error][in function \"maxspeedup\"] Overflow: the maximum_speedup is beyond the upper bound 10. Stop calculating."
			ret_maxspeedup=-1
			return
		else
			step=-1
			myscale=1
			min=1
			max=10
			basesu=0
			while [[ $step -le $acc_maxsu ]]; do
				mid=$((($max-$min)/2+$min))
				su=`echo "scale=$ACC;$basesu+$myscale*$mid"|bc`
				reci=`echo "scale=$ACC;1/$su"|bc`
				hpt_basic $row $col $reci $alpha MMTRX1 MMTRX2
				if [[ `echo "$ret_hpt_basic>$reliability"|bc` -eq 1 ]]; then
					min=$mid
				else
					max=$mid
				fi				

				if [[ $min -eq $(($max-1)) ]]; then
					basesu=`echo "scale=$ACC;$basesu+$min*$myscale"|bc`
					myscale=`echo "scale=$ACC;$myscale/10"|bc`
					step=$(($step+1))
					min=0
					max=10
				fi
			done
			ret_maxspeedup=`printf "%.${acc_maxsu}f" $basesu`
		fi
	fi
}

#########################################################################
#								Math Part END
#########################################################################






#########################################################################
#								Readfile Part
#########################################################################



read_config()
{
	# FUNCTION: set all the key parameters by reading the config file.
	# FORMAT: read_config cfg_file printinfo(display the config info when printinfo=1 and do not display when 0)
	# OUTPUT: $? (1 for Error and 0 for No_error)
	local cfgfile=$1
	local printinfo=$2

	if [[ $# -ne 2 ]]; then
		echo "[Error][in function \"read_config\"] Format error."
		return 1
	fi

	current_status=0
	AutoCompareOff=0
	i3=0
	len_trineseq=-1

	cnt=0
	matched=0
	ReadTriple=-1
	ReadDefaultMTRX=-1
	n_col_default=5

	str_1_1=HaveCompareGroup
	str_1_2=Name1
	str_1_3=LogGroup1
	str_1_4=Name2
	str_1_5=LogGroup2
	str_1_6=output_file
	str_1_7=DefaultBaselineName
	str_1_8=DefaultBaselineStatisticsLog
	str_2_1=repeat
	str_2_2=alpha_unibench
	str_2_3=speedup
	str_2_4=reliability

	printf "Reading NPT parameters at `date`...\n"
	while read LINE; do
		LINE=`echo $LINE`
		if [[ "$LINE" == *"##"* ]]; then
			LINE=${LINE%%##*}
			LINE=`echo $LINE`
		fi
		if [[ "$LINE" == "" ]]; then
			continue
		fi

		if [[ "$LINE" == "[CONFIG FILE END]" ]]; then
			current_status=-1
			break
		elif [[ $current_status -eq 0 ]]; then
			if [[ "$LINE" == "[Basic Info]" ]]; then
				current_status=1
			elif [[ "$LINE" == "[HPT Parameters]" ]]; then
				current_status=2
			elif [[ "$LINE" == "[Workload Settings]" ]]; then
				current_status=3
			elif [[ "$LINE" == "[Default Baseline Statistics]" ]]; then
				current_status=4
			fi
			continue
		elif [[ $current_status -eq 1 ]]; then
			if [[ "$LINE" == "[Basic Info End]" ]]; then
				current_status=0
				continue
			elif [[ "${LINE:0:${#str_1_1}}" == $str_1_1 ]]; then
				content=${LINE##HaveCompareGroup}
				content=`echo $content`
				content=${content##=}
				content=`echo $content`
				if [[ "$content" == "TRUE" ]] || [[ "$content" == "1" ]]; then
					AutoCompareOff=1
				else
					AutoCompareOff=0
				fi
			elif [[ "${LINE:0:${#str_1_2}}" == $str_1_2 ]]; then
				content=${LINE##Name1}
				content=`echo $content`
				content=${content##=}
				name1=`echo $content`
			elif [[ "${LINE:0:${#str_1_3}}" == $str_1_3 ]]; then
				if [ -d loggroup1 ]; then
					if [ -d loggroup1.old ]; then
						rm -rf loggroup1.old
					fi
					mv loggroup1 loggroup1.old
				fi
				mkdir loggroup1
				content=${LINE##LogGroup1}
				content=`echo $content`
				content=${content##=}
				content=`echo $content`
				for file in `ls $content`; do
					if [ -f $file ]; then
						cp $file loggroup1/
					fi
				done
			elif [[ "${LINE:0:${#str_1_4}}" == $str_1_4 ]]; then
				if [[ $AutoCompareOff -eq 1 ]]; then
					content=${LINE##Name2}
					content=`echo $content`
					content=${content##=}
					name2=`echo $content`
				fi
			elif [[ "${LINE:0:${#str_1_5}}" == $str_1_5 ]]; then
				if [[ $AutoCompareOff -eq 1 ]]; then
					if [ -d loggroup2 ]; then
						if [ -d loggroup2.old ]; then
							rm -rf loggroup2.old
						fi
						mv loggroup2 loggroup2.old
					fi
					mkdir loggroup2
					content=${LINE##LogGroup2}
					content=`echo $content`
					content=${content##=}
					content=`echo $content`
					for file in `ls $content`; do
						if [ -f $file ]; then
							cp $file loggroup2/
						fi
					done
				#	eval loggroup2=(`ls $content`)
				fi
			elif [[ "${LINE:0:${#str_1_7}}" == $str_1_7 ]]; then
				if [[ $AutoCompareOff -eq 0 ]]; then
					content=${LINE##DefaultBaselineName}
					content=`echo $content`
					content=${content##=}
					name2=`echo $content`
				fi
			elif [[ "${LINE:0:${#str_1_8}}" == $str_1_8 ]]; then
				if [[ $AutoCompareOff -eq 0 ]]; then
					content=${LINE##DefaultBaselineStatisticsLog}
					content=`echo $content`
					content=${content##=}
					loggroup2=`echo $content`
				fi
			elif [[ "${LINE:0:${#str_1_6}}" == $str_1_6 ]]; then
				content=${LINE##output_file}
				content=`echo $content`
				content=${content##=}
				outputfile=`echo $content`
			fi
		elif [[ $current_status -eq 2 ]]; then
			if [[ "$LINE" == "[HPT Parameters End]" ]]; then
				current_status=0
				continue
			elif [[ "${LINE:0:${#str_2_1}}" == $str_2_1 ]]; then
				content=${LINE##repeat}
				content=`echo $content`
				content=${content##=}
				repeat=`echo $content`
			elif [[ "${LINE:0:${#str_2_2}}" == $str_2_2 ]]; then
				content=${LINE##alpha_unibench}
				content=`echo $content`
				content=${content##=}
				alpha_uni=`echo $content`
			elif [[ "${LINE:0:${#str_2_3}}" == $str_2_3 ]]; then
				content=${LINE##speedup}
				content=`echo $content`
				content=${content##=}
				content=`echo $content`
				eval speedup=(\$content)
			elif [[ "${LINE:0:${#str_2_4}}" == $str_2_4 ]]; then
				content=${LINE##reliability}
				content=`echo $content`
				content=${content##=}
				content=`echo $content`
				eval reliability=(\$content)
			fi
		elif [[ $current_status -eq 3 ]]; then
			if [[ "$LINE" == "[Workload Settings End]" ]]; then
				len_trineseq=$i3
				current_status=0
				continue
			else
				trine=${LINE%% *}
				switch=${LINE##* }
				if [[ $switch -eq 1 ]]; then
					trineseq[$i3]=$trine
					i3=$(($i3+1))
				fi
			fi
		fi

	done<$cfgfile

	if [[ $repeat -le 2 ]]; then
		echo "[Warning] Then uni-bench level test will be skipped because of the small sample size(n_col<3)"
		alpha_uni=NaN
	elif [[ `echo "$alpha_uni!=0.1"|bc` -eq 1 ]] && [[ $repeat -lt 5 ]]; then
		echo "[Warning] The alpha value of uni-bench level is set to 0.1 by force because of the small sample size(n_col<=5)"
		alpha_uni=0.1
	fi

	printf "Complete reading NPT parameters at `date`\n\n"

	if [[ printinfo -eq 1 ]]; then
		check_cfg_info
	fi

	return 0
}

readmatrix()
{
	# FUNCTION: fill the matrix "var_matrix" using the data from all the log file under the directory "filegroup_dir".
	#			Only data among the list "seq_trine" will be collected. The size of the matrix is row*col.
	# FORMAT: readmatrix row col seq_trine filegroup_dir var_matrix
	# OUTPUT: $? (1 for Error and 0 for No_error) (var_matrix)
	local i; local j; local seq_trine; local seq_file; local cnt; local flag_break=0; local filename
	local row=$1
	local col=$2
	eval seq_trine=(\${$3[@]})
#	eval seq_file=(\${$4[@]})
	local filegroup_dir=$4
	local matrixname=$5
	local gotcha=0
	local totalcnt=0

	echo "Reading $filegroup_dir at `date`..."
	grep -E '\[PARSEC\]\ \[==========\ Running\ benchmark\ |real' $filegroup_dir/* >loadmatrix.tmp

	local str1="[PARSEC] [========== Running benchmark "
	if [[ ${#seq_trine[@]} -ne $row ]]; then
		echo "[Error][in function \"readmatrix\"] Variant \"row\" does not match!"
		return 1
	fi
	for i in `seq 0 $(($row-1))`; do
		cnt[$i]=0
	done

	#echo "trineseq(${#trineseq[@]}):${trineseq[@]}"
	while read LINE; do
		filename=${LINE%\.log*}
		thread=${filename##*/}
		thread=${thread#*run_}
		thread=${thread%%_*}
		inputsize=${filename##*/}
		inputsize=${inputsize#*run_*_}
		inputsize=${inputsize%%_*}
		LINE=${LINE#*\.log:}
		#echo thread=$thread, inputsize=$inputsize
		#echo ${LINE:0:${#str1}}  VS. $str1
		if [[ "${LINE:0:${#str1}}" == "$str1" ]]; then

			if [[ $gotcha -eq 1 ]]; then
				echo "[Error][in function \"readmatrix\"] Incorrect log file format!"
				return 1
			fi

			tmpstr=`expr match "$LINE" '.\+benchmark\ \(.\+\)\ \[.*'`
			tmpstr="$tmpstr-$inputsize-$thread"
			for j in `seq 0 $(($row-1))`; do
				if [[ $tmpstr == ${seq_trine[$j]} ]] && [[ ${cnt[$j]} -lt $col ]]; then
					pos=$(($col*$j+${cnt[$j]}))
					jth=$j
					gotcha=1
					break
				fi
			done
		elif [[ `expr substr "$LINE" 1 4` == "real" ]] && [[ $gotcha -eq 1 ]]; then
			LINE=`echo $LINE`
			mvalue=`expr match "$LINE" '.\+\ \(.\+\)m.*'`
			svalue=`expr match "$LINE" '.\+*m\(.\+\)s.*'`
			timevalue=`echo "scale=3;$mvalue*60+$svalue"|bc`
			eval $matrixname[$pos]=$timevalue
			totalcnt=$((1+$totalcnt))
			cnt[$jth]=$((1+${cnt[$jth]}))
			gotcha=0
		fi
		if [[ $totalcnt -eq $(($row*$col)) ]]; then
			flag_break=1
			break
		fi
	done <loadmatrix.tmp
	rm loadmatrix.tmp

	# check:
	for i in `seq 0 $(($row-1))`; do
		if [[ ${cnt[$i]} -ne $col ]]; then
			echo "[Error][in function \"readmatrix\"] Insufficient data!"
			return 1
		fi
	done
	echo "Complete reading $filegroup_dir at `date`"
}

read_default()
{
	# FUNCTION: read the default baseline matrix from file "filename" to matrix "matrixname".
	# FORMAT: read_default n_col seq_trine filename matrixname
	# OUTPUT: (matrixname)

	local seq_trine; local trine; local numseq; local i; local tmp; local cnt=0; local val
	#local j; local nt=12
	local n_col=$1
	eval seq_trine=(\${$2[@]})
	local filename=$3

	if [[ -f $filename ]]; then
		echo "Loading default baseline matrix..."
	else
		echo "[Error][in function \"read_default\"] Can not locate $filename!"
		return
	fi

	while read trine numseq; do
		for i in `seq 0 $((${#seq_trine[@]}-1))`; do
			if [ "${seq_trine[$i]}" == $trine ]; then
				cnt=$(($cnt+1))
				tmp=0
				for val in $numseq; do
					eval $4[$(($i*$n_col+$tmp))]=$val
					tmp=$(($tmp+1))
					if [[ $tmp -ge $n_col ]]; then
						break
					fi
				done

				break
			fi
		done
		if [[ $cnt -eq ${#seq_trine[@]} ]]; then
			return
		fi
	done <$filename

	echo "[Error][in function \"read_default\"] Can not locate all the workload when searching the baseline file."
}


#########################################################################
#							Readfile Part End
#########################################################################


check_cfg_info()
{
	# FUNCTION: print the config parameters.
	# FORMAT: check_cfg_info
	# OUTPUT: (None)

	echo "===========[Parameter Info Checklist]=========="
	echo "HaveCompareGroup: $AutoCompareOff"
	echo "Machine1: $name1"
	echo "Machine2: $name2"
	echo "DataLog1: `ls loggroup1`"
	echo 
	if [ "$AutoCompareOff" == "1" ] || [ "$AutoCompareOff" == "TRUE" ]; then
		echo "DataLog2: `ls loggroup2`"
	else
		echo "DataLog2: ${loggroup2}"
	fi
	echo "Outputfile: $outputfile"
	echo "repeat: $repeat"
	echo "alpha: $alpha_uni"
	if [[ ${#speedup[@]} -eq 0 ]]; then
		echo "Speedup: None"
	else
		echo "Speedup(${#speedup[@]}):${speedup[@]}"
	fi
	if [[ ${#reliability[@]} -eq 0 ]]; then
		echo "Reliability: None"
	else
		echo "Reliability(${#reliability[@]}):${reliability[@]}"
	fi
	echo "Benchmark-Inputsize-Nthread(${#trineseq[@]} in total):"
	echo ${trineseq[@]}
	echo "================[Checklist End]================"
	echo
}


main()
{
	# FUNCTION: the whole HPTest, including reading config file, reading data matrices, reliability computing, 
	#			and maximum speedup computing.
	# FORMAT:
	# OUTPUT:

	printf "\n=================================================================\n"
	printf "=========================    HPT   ==============================\n"
	printf "=================================================================\n\n"
	if [[ $# -ne 1 ]]; then
		echo "[Error][in function \"main\"] the standard format should be \"bash hpt.sh cfgfile\""
		return 1
	fi

	read_config $1 1
	if [[ $? -eq 1 ]]; then
		return 1
	fi
	echo "=================================================================" >$outputfile
	echo "=========================    HPT   ==============================" >>$outputfile
	echo "=================================================================" >>$outputfile
	echo "" >>$outputfile
	echo "[Date] `date`" >>$outputfile
	echo "" >>$outputfile
	check_cfg_info >>$outputfile

	row=${#trineseq[@]}
	if [[ AutoCompareOff -eq 0 ]] && [[ $repeat -gt $NCOL_BASELINE ]]; then
		echo "[Overflow] repeat times(which is $repeat) should be no larger than NCOL_BASELINE($NCOL_BASELINE) when using default baseline as comparison."
		return
	fi

	readmatrix ${#trineseq[@]} $repeat trineseq loggroup1 matrix_a
	if [[ $? -eq 1 ]]; then
		return
	fi
	if [[ AutoCompareOff -eq 1 ]]; then
		readmatrix ${#trineseq[@]} $repeat trineseq loggroup2 matrix_b
		if [[ $? -eq 1 ]]; then
			return
		fi
	else
		read_default $repeat trineseq $loggroup2 matrix_b
		#read_default $repeat $loggroup2 matrix_b
	fi

	display_matrix $row $repeat matrix_a $name1
	display_matrix $row $repeat matrix_b $name2
	display_matrix $row $repeat matrix_a $name1 >>$outputfile
	display_matrix $row $repeat matrix_b $name2 >>$outputfile
	echo "" >>$outputfile

	printf "\nAnalyzing and calculating, this may take a few minutes...\n\n"
	#	hpt_basic $row $repeat 1 $alpha_uni matrix_a matrix_b
	time hpt_basic $row $repeat 1 $alpha_uni matrix_a matrix_b

	printf "===========================================================================================\n"
	local n_mul=$((${#speedup[@]}+($acc_maxsu+1)*4*${#reliability[@]}))
	printf "[Time estimating] The whole process will complete in about $n_mul times the above time.\n"
	printf "===========================================================================================\n\n"

	if [[ `echo "$ret_hpt_wp<$ret_hpt_wn"|bc` -eq 1 ]]; then
		tmp_reli=`echo "scale=4;1-$ret_hpt_basic"|bc`
		t_name1="machine_A($name1)"
		t_name2="machine_B($name2)"
		better=1
	else
		tmp_reli=`echo "scale=4;$ret_hpt_basic"|bc`
		t_name1="machine_B($name2)"
		t_name2="machine_A($name1)"
		better=0
	fi
	printf "[Comparison][original]\n$t_name1 is better than $t_name2 with reliability %.4f.\n\n" $tmp_reli
	printf "[Comparison][original]\n$t_name1 is better than $t_name2 with reliability %.4f.\n\n" $tmp_reli >>$outputfile

	for i in `seq 0 $((${#speedup[@]}-1))`; do
		su=${speedup[$i]}
		if [[ $better -eq 1 ]]; then
			hpt_basic $row $repeat $su $alpha_uni matrix_a matrix_b
			tmp_reli=`echo "scale=4;1-$ret_hpt_basic"|bc`
		else
			hpt_basic $row $repeat $su $alpha_uni matrix_b matrix_a
			tmp_reli=`echo "scale=4;1-$ret_hpt_basic"|bc`
		fi
		printf "[Comparison][speedup=%.4f]\n" $su
		printf "The reliability of that $t_name1 outperforms $t_name2 with speedup %.4f is %.4f.\n\n" $su $tmp_reli
		printf "[Comparison][speedup=%.4f]\n" $su >>$outputfile
		printf "The reliability of that $t_name1 outperforms $t_name2 with speedup %.4f is %.4f.\n\n" $su $tmp_reli >>$outputfile
	done

	for i in `seq 0 $((${#reliability[@]}-1))`; do
		reli=${reliability[$i]}
		maxspeedup $row $repeat $reli $alpha_uni $better matrix_a matrix_b
		printf "[Comparison][reliability=%.4f]\n" $reli
		printf "[Comparison][reliability=%.4f]\n" $reli >>$outputfile
		if [[ `echo "$ret_maxspeedup>0"|bc` -eq 1 ]]; then
			printf "The maximum speedup of $t_name1 over $t_name2 is %.${acc_maxsu}f with the given reliability %.4f.\n\n" $ret_maxspeedup $reli
			printf "The maximum speedup of $t_name1 over $t_name2 is %.${acc_maxsu}f with the given reliability %.4f.\n\n" $ret_maxspeedup $reli >>$outputfile
		else
			printf "Skipped\n\n"
			printf "Skipped\n\n" >>$outputfile
		fi
	done

	printf "\n\n[HPT COMPLETED] `date`\n" >>$outputfile

	if [[ -f loadmatrix.tmp ]]; then
		rm loadmatrix.tmp
	fi
}


main $1

