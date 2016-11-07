# Perform an interpolating search without/with a maximum cache size, and check for consistent results

echo "=== Create search setup with 3 segments ==="
set -x
${builddir}/lalapps_WeaveSetup --first-segment=1122332211/90000 --segment-count=3 --detectors=H1,L1 --output-file=WeaveSetup.fits
set +x
echo

echo "=== Perform interpolating search without a maximum cache size ==="
set -x
${builddir}/lalapps_Weave --cache-max-size=0 --output-file=WeaveOutNoMax.fits \
    --output-toplist-limit=5000 --output-misc-info --setup-file=WeaveSetup.fits \
    --sft-timebase=1800 --sft-noise-psd=1,1 --sft-noise-rand-seed=3456 \
    --sft-timestamps-files=${srcdir}/timestamps-irregular.txt,${srcdir}/timestamps-regular.txt \
    --alpha=0.9/1.4 --delta=-1.2/2.3 --freq=50.5/1e-4 --f1dot=-1.5e-9,0 --semi-max-mismatch=5 --coh-max-mismatch=0.3
set +x
echo

echo "=== Check average number of semicoherent templates per dimension is more than one"
set -x
for dim in SSKYA SSKYB NU0DOT NU1DOT; do
    ${fitsdir}/lalapps_fits_header_getval "WeaveOutNoMax.fits[0]" "SEMIAVG ${dim}" > tmp
    semi_avg_ntmpl_dim=`cat tmp | xargs printf "%d"`
    expr ${semi_avg_ntmpl_dim} '>' 1
done
set +x
echo

echo "=== Perform interpolating search with a maximum cache size ==="
set -x
${builddir}/lalapps_Weave --cache-max-size=10 --output-file=WeaveOutMax.fits \
    --output-toplist-limit=5000 --output-misc-info --setup-file=WeaveSetup.fits \
    --sft-timebase=1800 --sft-noise-psd=1,1 --sft-noise-rand-seed=3456 \
    --sft-timestamps-files=${srcdir}/timestamps-irregular.txt,${srcdir}/timestamps-regular.txt \
    --alpha=0.9/1.4 --delta=-1.2/2.3 --freq=50.5/1e-4 --f1dot=-1.5e-9,0 --semi-max-mismatch=5 --coh-max-mismatch=0.3
set +x
echo

for seg in 1 2 3; do

    echo "=== Segment #${seg}: Check that number of computed coherent results are equal ==="
    set -x
    ${fitsdir}/lalapps_fits_table_list "WeaveOutNoMax.fits[per_seg_info][col coh_n1comp][#row == ${seg}]" > tmp
    coh_n1comp_no_max=`cat tmp | sed "/^#/d" | xargs printf "%d"`
    ${fitsdir}/lalapps_fits_table_list "WeaveOutMax.fits[per_seg_info][col coh_n1comp][#row == ${seg}]" > tmp
    coh_n1comp_max=`cat tmp | sed "/^#/d" | xargs printf "%d"`
    expr ${coh_n1comp_no_max} '=' ${coh_n1comp_max}
    set +x
    echo

    echo "=== Segment #${seg}: Check that search without a maximum cache size did not recompute results ==="
    set -x
    ${fitsdir}/lalapps_fits_table_list "WeaveOutNoMax.fits[per_seg_info][col coh_nrecomp][#row == ${seg}]" > tmp
    coh_nrecomp_no_max=`cat tmp | sed "/^#/d" | xargs printf "%d"`
    expr ${coh_nrecomp_no_max} '=' 0
    set +x
    echo

    echo "=== Segment #${seg}: Check that search with a maximum cache size did recompute results ==="
    set -x
    ${fitsdir}/lalapps_fits_table_list "WeaveOutMax.fits[per_seg_info][col coh_nrecomp][#row == ${seg}]" > tmp
    coh_nrecomp_max=`cat tmp | sed "/^#/d" | xargs printf "%d"`
    expr ${coh_nrecomp_max} '>' 0
    set +x
    echo

done

echo "=== Compare F-statistics from lalapps_Weave without/with a maximum cache size ==="
set -x
LAL_DEBUG_LEVEL="${LAL_DEBUG_LEVEL},info"
${builddir}/lalapps_WeaveCompare --setup-file=WeaveSetup.fits --output-file-1=WeaveOutNoMax.fits --output-file-2=WeaveOutMax.fits
set +x
echo
