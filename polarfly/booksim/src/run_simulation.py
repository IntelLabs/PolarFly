import subprocess
import sys

#loads = [0.01,0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8,0.9,0.95]
loads = [0.8,0.85,0.9,0.95]

# Read arguments
if len(sys.argv) < 4:
	print("Usage: run_simulation.py <graph> <traffic> <routing> [adaptive-th] [load]")
	sys.exit()
graph = sys.argv[1]
traffic = sys.argv[2]
routing = sys.argv[3]
adaptive_th = sys.argv[4] if len(sys.argv) > 4 else "4"
one_load = sys.argv[5] if len(sys.argv) > 5 else None
po2 = "yes" if traffic in ["bitrev","shuffle"] else "no"

loads = loads if one_load == None else [float(one_load)]
load_txt = "" if one_load == None else ("-" + str(one_load))

# Prepare files
cfg_file_name = graph + ".conf"
res_file_name = "results/results-%s-%s-%s-%s%s.csv" % (graph, traffic, routing, adaptive_th, load_txt)
log_file_name = "logs/log-%s-%s-%s-%s%s.log" % (graph, traffic, routing, adaptive_th, load_txt)


with open(res_file_name, "w") as res_file:
	print("=== %s - %s - %s - %s ===" % (graph, traffic, routing, adaptive_th))
	with open(log_file_name, "w") as log_file:
		log_file.write("Creating new log for %s - %s - %s - %s" % (graph, traffic, routing, adaptive_th))
	for load in loads:
		print("-> " + str(load) + " => ", end = '')
		# Read update configuration
		with open(cfg_file_name, "r") as cfg_file:
			lines = cfg_file.readlines()
		lines[0] = "injection_rate = " + str(load) + ";\n"
		lines[1] = "traffic = " + traffic + ";\n"
		lines[2] = "routing_function = " + routing + ";\n"
		lines[3] = "use_size_power_of_two = " + po2 + ";\n"
		lines[4] = "adaptive_threshold = " + adaptive_th + ";\n"
		with open(cfg_file_name, "w") as cfg_file:
			cfg_file.writelines(lines)
		# Start simulation and capture output
		proc = subprocess.Popen(['./booksim', cfg_file_name], stdout=subprocess.PIPE)
		out = proc.stdout.read()
		# Write lowest 50 lines of output to logfile
		out_list = str(out)[1:-1].split("\\n")[-50:]
		out_str = ""
		for line in out_list:
			out_str += line + "\n"	
		with open(log_file_name, "a") as log_file:
			log_file.write("\n\n********** %s - %s - %s - %s - %f **********\n\n" % (graph, traffic, routing, adaptive_th, load))	
			log_file.write(out_str)
		if "unstable" in out_str:
			res_file.write(str(load) + ", " + str(500) + "\n")
			print("Unstable simulation")
			break
		# Capture result 
		vals = [-1,-1,-1,-1,-1,-1,-1]
		lines = [-34,-3,-4,-5,-6,-7,-8]
		try:
			for i in range(len(lines)):	
				tmp = out_list[lines[i]].split(" ")	
				for j in range(len(tmp)-1):
					if tmp[j] == "=":
						vals[i] = float(tmp[j+1])
						break
		except:
			print("Exception while reading BookSim output")
		res_file.write("%f, %f, %f, %f, %f, %f, %f, %f\n" % tuple([load] + vals))
		print(vals)
		# Stop if too many cycles
		if vals[0] <= 0 or vals[0] > 50.0:
			break
	res_file.write("\n")
