#!/usr/bin/python
from __future__ import print_function

import sqlite3
from glue.ligolw import dbtables 
from glue.ligolw import table
from pylal import SnglInspiralUtils
from pylal import db_thinca_rings
from pylal import git_version
from time import clock,time
from optparse import *
import glob
import sys

usage="""
example command line:
%prog --instruments=H1,L1 database1.sqlite database2.sqlite etc.
or, if you are brave:
%prog --instruments=H1,L1 *.sqlite
this code turns sqlite databases into .pat files for MVSC
each row in the .pat file contains a vector of parameters that characterize the double coincident trigger
"""

__author__ = "Kari Hodge <khodge@ligo.caltech.edu>"

parser=OptionParser(usage=usage,version=git_version.verbose_msg)
parser.add_option("", "--number", default=10, type="int", help="number for round robin")
parser.add_option("", "--instruments", help="pair that you want to get like H1,L1")
parser.add_option("", "--trainingstr", default="training", help="the string in the output .pat files that indicates it will be used as the training set in SprBaggerDecisionTreeApp") 
parser.add_option("", "--testingstr", default="testing", help="the string in the output .pat files that indicates it will be used as the testing set in SprOutputWriterApp") 
parser.add_option("", "--zerolagstr", default="zerolag", help="the string in the output .pat files that indicates it contains zerlag data, which will be run through SprOutputWriterApp") 
(opts,databases)=parser.parse_args()
ifos=opts.instruments.strip().split(',')
ifos.sort()

time1=time()

def calc_effective_snr(snr, chisq, chisq_dof, fac=250.0):
  return snr/ (1 + snr**2/fac)**(0.25) / (chisq/(2*chisq_dof - 2) )**(0.25)

injections = []
injections_info = []
zerolag = []
zerolag_info = []
timeslides = []
timeslides_info = []

for database in databases:
  local_disk = None #"/tmp"
  working_filename = dbtables.get_connection_filename(database, tmp_path = local_disk, verbose = True)
  connection = sqlite3.connect(working_filename)
  dbtables.DBTable_set_connection(connection)
  xmldoc = dbtables.get_xml(connection)
  cursor = connection.cursor()
  # to determine whether or not the database is full of timeslides/zerolag or injections, check if there exists a sim_inspiral table 
  try:
    sim_inspiral_table = dbtables.lsctables.SimInspiralTable.get_table(xmldoc)
    is_injections = True
  except ValueError:
    is_injections = False

  if is_injections:
    def calc_delta_t_inj(trigger1_end_time, trigger1_end_time_ns, trigger2_end_time, trigger2_end_time_ns):
      try:
        return abs((trigger1_end_time - trigger2_end_time) + (trigger1_end_time_ns - trigger2_end_time)*1e-9)
      except: print("calc_delta_t_inj() failed")
    connection.create_function("calc_delta_t_inj", 4, calc_delta_t_inj)
    connection.create_function("calc_effective_snr", 3, calc_effective_snr)
    
    #FIXME: look up coinc_definer_id from cefinition in pylal
    for values in connection.cursor().execute("""
    SELECT
      coinc_inspiral.coinc_event_id,
      calc_delta_t_inj(snglA.end_time, snglA.end_time_ns, snglB.end_time, snglB.end_time_ns),
      abs(2*(snglA.mchirp - snglB.mchirp)/(snglA.mchirp+snglB.mchirp)),
      abs(2*(snglA.eta - snglB.eta)/(snglA.eta+snglB.eta)),
      snglA.snr,
      snglB.snr,
      snglA.chisq,
      snglB.chisq,
      calc_effective_snr(snglA.snr, snglA.chisq, snglA.chisq_dof),
      calc_effective_snr(snglB.snr, snglB.chisq, snglB.chisq_dof),
      snglA.rsqveto_duration,
      snglB.rsqveto_duration,
      snglA.bank_chisq,
      snglB.bank_chisq,
      snglA.cont_chisq,
      snglB.cont_chisq
    FROM
      coinc_inspiral 
      JOIN coinc_event_map AS mapA ON (mapA.coinc_event_id == coinc_inspiral.coinc_event_id)
      JOIN coinc_event_map AS mapB ON (mapB.coinc_event_id == coinc_inspiral.coinc_event_id)
      JOIN sngl_inspiral AS snglA ON (snglA.event_id == mapA.event_id)
      JOIN sngl_inspiral AS snglB ON (snglB.event_id == mapB.event_id)
      JOIN coinc_event_map AS mapC ON (mapC.event_id == coinc_inspiral.coinc_event_id)
      JOIN coinc_event_map AS mapD ON (mapD.coinc_event_id == mapC.coinc_event_id)
      JOIN sim_inspiral ON (sim_inspiral.simulation_id == mapD.event_id)
      JOIN coinc_event AS sim_coinc_event ON (sim_coinc_event.coinc_event_id == mapD.coinc_event_id)
      JOIN coinc_event AS insp_coinc_event ON (insp_coinc_event.coinc_event_id == mapA.coinc_event_id)
      JOIN coinc_definer ON (coinc_definer.coinc_def_id == sim_coinc_event.coinc_def_id)
    WHERE
      coinc_definer.search == 'inspiral'
      AND coinc_definer.search_coinc_type == 2
      AND mapA.table_name == 'sngl_inspiral'
      AND mapB.table_name == 'sngl_inspiral'
      AND mapC.table_name == 'coinc_event'
      AND mapD.table_name == 'sim_inspiral'
      AND snglA.ifo == ?
      AND snglB.ifo == ?
      """, (tuple(ifos)) ):
        injections.append(values[1:] + (1,))
        injections_info.append([values[0], database])
    dbtables.put_connection_filename(database, working_filename, verbose = True)

  if not is_injections:  
    rings = db_thinca_rings.get_thinca_rings_by_available_instruments(connection)
    offset_vectors = dbtables.lsctables.TimeSlideTable.get_table(dbtables.get_xml(connection)).as_dict()
  
    def calc_delta_t(trigger1_ifo, trigger1_end_time, trigger1_end_time_ns, trigger2_ifo, trigger2_end_time, trigger2_end_time_ns, time_slide_id, rings = rings, offset_vectors = offset_vectors):
      print("calculating delta_t", file=sys.stderr)
      trigger1_true_end_time = dbtables.lsctables.LIGOTimeGPS(trigger1_end_time, trigger1_end_time_ns)
      trigger2_true_end_time = dbtables.lsctables.LIGOTimeGPS(trigger2_end_time, trigger2_end_time_ns)
      # find the instruments that were on at trigger 1's end time and
      # find the ring that contains this trigger
      try:
        [ring] = [segs[segs.find(trigger1_end_time)] for segs in rings.values() if trigger1_end_time in segs]
      except ValueError:
        # FIXME THERE SEEMS TO BE A BUG IN  THINCA!  Occasionally thinca records a trigger on the upper boundary
        # of its ring.  This would make it outside the ring which is very problematic.  It needs to be fixed in thinca
        # for now we'll allow the additional check that the other trigger is in the ring and use it.
          print("trigger1 found not on a ring, trying trigger2", file=sys.stderr)
          [ring] = [segs[segs.find(trigger2_end_time)] for segs in rings.values() if trigger2_end_time in segs]
      # now we can unslide the triggers on the ring
      try:
        trigger1_true_end_time = SnglInspiralUtils.slideTimeOnRing(trigger1_true_end_time, offset_vectors[time_slide_id][trigger1_ifo], ring)
        trigger2_true_end_time = SnglInspiralUtils.slideTimeOnRing(trigger2_true_end_time, offset_vectors[time_slide_id][trigger2_ifo], ring)
        out = abs(trigger1_true_end_time - trigger2_true_end_time)
        return float(out)
      except:
        print("calc delta t failed",trigger1_true_end_time, trigger2_true_end_time, ring)
        return float(abs(trigger1_true_end_time - trigger2_true_end_time)) % 1
  
    
    connection.create_function("calc_delta_t", 7, calc_delta_t)
    connection.create_function("calc_effective_snr", 3, calc_effective_snr)
    
   
    #FIXME: look up coinc_definer_id from cefinition in pylal
    for values in connection.cursor().execute("""
    SELECT
      coinc_inspiral.coinc_event_id,
      calc_delta_t(snglA.ifo, snglA.end_time, snglA.end_time_ns, snglB.ifo, snglB.end_time, snglB.end_time_ns, coinc_event.time_slide_id),
      abs(2*(snglA.mchirp - snglB.mchirp)/(snglA.mchirp+snglB.mchirp)),
      abs(2*(snglA.eta - snglB.eta)/(snglA.eta+snglB.eta)),
      snglA.snr,
      snglB.snr,
      snglA.chisq,
      snglB.chisq,
      calc_effective_snr(snglA.snr, snglA.chisq, snglA.chisq_dof),
      calc_effective_snr(snglB.snr, snglB.chisq, snglB.chisq_dof),
      snglA.rsqveto_duration,
      snglB.rsqveto_duration,
      snglA.bank_chisq,
      snglB.bank_chisq,
      snglA.cont_chisq,
      snglB.cont_chisq,
      EXISTS (
        SELECT
          *
        FROM
          time_slide
        WHERE
          time_slide.time_slide_id == coinc_event.time_slide_id
          AND time_slide.offset != 0
      )
    FROM
      coinc_inspiral 
      JOIN coinc_event_map AS mapA ON (mapA.coinc_event_id == coinc_inspiral.coinc_event_id)
      JOIN coinc_event_map AS mapB ON (mapB.coinc_event_id == coinc_inspiral.coinc_event_id)
      JOIN sngl_inspiral AS snglA ON (snglA.event_id == mapA.event_id)
      JOIN sngl_inspiral AS snglB ON (snglB.event_id == mapB.event_id)
      JOIN coinc_event ON (mapA.coinc_event_id == coinc_event.coinc_event_id)
      JOIN coinc_definer ON (coinc_definer.coinc_def_id == coinc_event.coinc_def_id)
    WHERE
      coinc_definer.search == 'inspiral'
      AND coinc_definer.search_coinc_type == 0
      AND mapA.table_name == 'sngl_inspiral'
      AND mapB.table_name == 'sngl_inspiral'
      AND snglA.ifo == ?
      AND snglB.ifo == ?
      """, tuple(ifos) ):
        is_background = values[-1]
        values = values[:-1]
        if is_background:
          timeslides.append(values[1:] + (0,))
          timeslides_info.append([values[0], database])
        else:
          zerolag.append(values[1:] + (0,))
          zerolag_info.append([values[0], database])
    
    dbtables.put_connection_filename(database, working_filename, verbose = True)


# this part of the code writes the triggers' information into .pat files, in the format needed for SprBaggerDecisionTreeApp
# to get the MVSC rank for each timeslide and injection, we do a round-robin of training and testing, with the number of rounds determined by opts.number
# for example,  if opts.number is 10, each round will train a random forest of bagged decision trees on 90% of the timeslides and injections
# then we'd run the remaining 10% through the trained forest to get their MVSC rank
# in this case, we'd do this 10 times, ensuring that every timeslide and injection gets ranked 
Nrounds = opts.number
Ninj = len(injections)
Nslide = len(timeslides)
print(injections, file=sys.stderr)
Nparams = len(injections[0][:]) - 1

trstr = opts.trainingstr
testr = opts.testingstr
zlstr = opts.zerolagstr

def open_file_write_headers(filetype, set_num, ifos, Nparams=Nparams):
  f = open(''.join(ifos) + '_set' + str(set_num) + '_' + str(filetype) +  '.pat', 'w')
  f.write(str(Nparams) + '\n')
  f.write("delta_t ab_dmchirp_rel ab_deta_rel a_snr b_snr a_chisq b_chisq a_effective_snr b_effective_snr a_rsq_veto_duration b_rsq_veto_duration a_bank_chisq b_bank_chisq a_cont_chisq b_cont_chisq \n")
  return f
for i in range(Nrounds):
  f_training = open_file_write_headers(trstr, i, ifos)
  f_testing = open_file_write_headers(testr, i, ifos)
  f_testing_info=open(''.join(ifos) + '_set' + str(i) + '_' + str(testr) + '_info.pat', 'w')
  set_inj = list(injections)
  set_inj_info = list(injections_info)
  print(len(set_inj))
  print(len(set_inj_info))
  set_slide = list(timeslides)
  set_slide_info = list(timeslides_info)
  # get 10% of the timeslides and injections, which you will run through the forest that you've trained on the other 90%
  # FIX me: shuffle my list
  set_i_inj= set_inj[i*Ninj/Nrounds : (i+1)*Ninj/Nrounds]
  set_i_inj_info= set_inj_info[i*Ninj/Nrounds : (i+1)*Ninj/Nrounds]
  set_i_slide = set_slide[i*Nslide/Nrounds : (i+1)*Nslide/Nrounds]
  set_i_slide_info = set_slide_info[i*Nslide/Nrounds : (i+1)*Nslide/Nrounds]
  print(len(set_i_inj))
  print(len(set_i_inj_info))
  #print set_i_inj_info
  for row in set_i_inj:
    f_testing.write("%s\n" % " ".join(map(str,row)))
  for row in set_i_inj_info:
    f_testing_info.write("%s\n" % " ".join(map(str,row)))
  for row in set_i_slide:
    f_testing.write("%s\n" % " ".join(map(str,row)))
  for row in set_i_slide_info:
    f_testing_info.write("%s\n" % " ".join(map(str,row)))
  # delete the 10%, and save the remaining 90% into the training file
  del(set_inj[i*Ninj/Nrounds : (i+1)*Ninj/Nrounds])
  del(set_slide[i*Nslide/Nrounds : (i+1)*Nslide/Nrounds])
  for row in set_inj:
    f_training.write("%s\n" % " ".join(map(str,row)))
  for row in set_slide:
    f_training.write("%s\n" % " ".join(map(str,row)))


f_zerolag=open(''.join(ifos) + '_' + str(zlstr) + '.pat','w')
f_zerolag.write(str(len(injections[0][:])-1) + '\n')
f_zerolag.write("delta_t ab_dmchirp_rel ab_deta_rel a_snr b_snr a_chisq b_chisq a_effective_snr b_effective_snr a_rsq_veto_duration b_rsq_veto_duration a_bank_chisq b_bank_chisq a_cont_chisq b_cont_chisq \n")
for row in zerolag:
  f_zerolag.write("%s\n" % " ".join(map(str,row)))
f_zerolag_info=open(''.join(ifos) + '_' + str(zlstr) + '_info.pat', 'w')
for row in zerolag_info:
  f_zerolag_info.write("%s\n" % " ".join(map(str,row)))

time2=time()
elapsed_time=time2-time1
print("elapsed time:", elapsed_time)
