
#include "chain.h"
#include <string.h>

/*************************************************************************
 *   Chain Class Member Functions
 *************************************************************************/

// Standard constructor
TChain::TChain(unsigned int _N, unsigned int _capacity)
	: stats(_N)
{
	N = _N;
	length = 0;
	total_weight = 0;
	set_capacity(_capacity);
	
	// Initialize min/max coordinates
	x_min.reserve(N);
	x_max.reserve(N);
	for(unsigned int i=0; i<N; i++) {
		x_min.push_back(std::numeric_limits<double>::infinity());
		x_max.push_back(-std::numeric_limits<double>::infinity());
	}
}

// Copy constructor
TChain::TChain(const TChain& c)
	: stats(1)
{
	stats = c.stats;
	x = c.x;
	L = c.L;
	w = c.w;
	total_weight = c.total_weight;
	N = c.N;
	length = c.length;
	capacity = c.capacity;
	x_min = c.x_min;
	x_max = c.x_max;
}

// Construct the string from file
TChain::TChain(std::string filename, bool reserve_extra)
	: stats(1)
{
	bool load_success = load(filename, reserve_extra);
	if(!load_success) {
		std::cout << "Failed to load " << filename << " into chain." << std::endl;
	}
}

TChain::~TChain() {}

void TChain::add_point(double* element, double L_i, double w_i) {
	stats(element, (unsigned int)w_i);
	for(unsigned int i=0; i<N; i++) {
		x.push_back(element[i]);
		if(element[i] < x_min[i]) { x_min[i] = element[i]; }
		if(element[i] > x_max[i]) { x_max[i] = element[i]; }
	}
	L.push_back(L_i);
	w.push_back(w_i);
	total_weight += w_i;
	length += 1;
}

void TChain::clear() {
	x.clear();
	L.clear();
	w.clear();
	stats.clear();
	total_weight = 0;
	length = 0;
	
	// Reset min/max coordinates
	for(unsigned int i=0; i<N; i++) {
		x_min[i] = std::numeric_limits<double>::infinity();
		x_max[i] = -std::numeric_limits<double>::infinity();
	}
}

void TChain::set_capacity(unsigned int _capacity) {
	capacity = _capacity;
	x.reserve(N*capacity);
	L.reserve(capacity);
	w.reserve(capacity);
}

unsigned int TChain::get_capacity() const {
	return capacity;
}

unsigned int TChain::get_length() const {
	return length;
}

double TChain::get_total_weight() const {
	return total_weight;
}

const double* TChain::get_element(unsigned int i) const {
	return &(x[i*N]);
}

double TChain::get_L(unsigned int i) const {
	return L[i];
}

double TChain::get_w(unsigned int i) const {
	return w[i];
}

double TChain::append(const TChain& chain, bool reweight, bool use_peak, double nsigma_max, double nsigma_peak, double chain_frac, double threshold) {
	assert(chain.N == N);	// Make sure the two chains have the same dimensionality
	
	// Weight each chain according to Bayesian evidence, if requested
	double a1 = 1.;
	double a2 = 1.;
	double lnZ = 0.;
	if(reweight) {
		double lnZ1 = chain.get_ln_Z_harmonic(use_peak, nsigma_max, nsigma_peak, chain_frac);
		double lnZ2 = get_ln_Z_harmonic(use_peak, nsigma_max, nsigma_peak, chain_frac);
		
		if(lnZ2 > lnZ1) {
			a2 = exp(lnZ1 - lnZ2) * total_weight / chain.total_weight;
			/*if(isnan(a2)) {
				std::cout << std::endl;
				std::cout << "NaN Error: a2 = " << a2 << std::endl;
				std::cout << "ln(Z1) = " << lnZ1 << std::endl;
				std::cout << "ln(Z2) = " << lnZ2 << std::endl;
				std::cout << "total_weight = " << total_weight << std::endl;
				std::cout << "chain.total_weight = " << chain.total_weight << std::endl;
				std::cout << std::endl;
			}*/
		} else {
			a1 = exp(lnZ2 - lnZ1) * chain.total_weight / total_weight;
			/*if(isnan(a1)) {
				std::cout << std::endl;
				std::cout << "NaN Error: a1 = " << a1 << std::endl;
				std::cout << "ln(Z1) = " << lnZ1 << std::endl;
				std::cout << "ln(Z2) = " << lnZ2 << std::endl;
				std::cout << "total_weight = " << total_weight << std::endl;
				std::cout << "chain.total_weight = " << chain.total_weight << std::endl;
				std::cout << std::endl;
			}*/
		}
		
		lnZ = log(a1/(a1+a2) * exp(lnZ2) + a2/(a1+a2) * exp(lnZ1));
	}
	
	if(reweight) { std::cout << "(a1, a2) = " << a1 << ", " << a2 << std::endl; }
	
	// Append the last chain to this one
	if(reweight && (a1 < threshold)) {
		x = chain.x;
		L = chain.L;
		w = chain.w;
		length = chain.length;
		capacity = chain.capacity;
		stats = chain.stats;
		total_weight = chain.total_weight;
		for(unsigned int i=0; i<N; i++) {
			x_max[i] = chain.x_max[i];
			x_min[i] = chain.x_min[i];
		}
	} else if(!(reweight && (a2 < threshold))) {
		if(capacity < length + chain.length) { set_capacity(1.5*(length + chain.length)); }
		std::vector<double>::iterator w_end_old = w.end();
		x.insert(x.end(), chain.x.begin(), chain.x.end());
		L.insert(L.end(), chain.L.begin(), chain.L.end());
		w.insert(w.end(), chain.w.begin(), chain.w.end());
		
		if(reweight) {
			std::vector<double>::iterator w_end = w.end();
			for(std::vector<double>::iterator it = w.begin(); it != w_end_old; ++it) {
				*it *= a1;
			}
			for(std::vector<double>::iterator it = w_end_old; it != w_end; ++it) {
				*it *= a2;
			}
		}
		
		stats *= a1;
		stats += a2 * chain.stats;
		length += chain.length;
		total_weight *= a1;
		total_weight += a2 * chain.total_weight;
		
		// Update min/max coordinates
		for(unsigned int i=0; i<N; i++) {
			if(chain.x_max[i] > x_max[i]) { x_max[i] = chain.x_max[i]; }
			if(chain.x_min[i] < x_min[i]) { x_min[i] = chain.x_min[i]; }
		}
	}
	//stats.clear();
	//for(unsigned int i=0; i<length; i++) {
	//	stats(get_element(i), 1.e10*w[i]);
	//}
	
	return lnZ;
}

const double* TChain::operator [](unsigned int i) {
	return &(x[i*N]);
}

void TChain::operator +=(const TChain& rhs) {
	append(rhs);
}

TChain& TChain::operator =(const TChain& rhs) {
	if(&rhs != this) {
		stats = rhs.stats;
		x = rhs.x;
		L = rhs.L;
		w = rhs.w;
		total_weight = rhs.total_weight;
		N = rhs.N;
		length = rhs.length;
		capacity = rhs.capacity;
		x_min = rhs.x_min;
		x_max = rhs.x_max;
	}
	return *this;
}


// A structure used to sort the elements of the chain
struct TChainSort {
	unsigned int index;
	double dist2;
	
	bool operator<(const TChainSort rhs) const { return dist2 < rhs.dist2; }	// Reversed so that the sort is in ascending order
};

//bool chainsortcomp(TChainSort a, TChainSort b) { return a.dist2 < b.dist2; }


// Uses a variant of the bounded harmonic mean approximation to determine the evidence.
// Essentially, the regulator chosen is an ellipsoid with radius nsigma standard deviations
// along each principal axis. The regulator is then 1/V inside the ellipsoid and 0 without,
// where V is the volume of the ellipsoid. In this form, the harmonic mean approximation
// has finite variance. See Gelfand & Dey (1994) and Robert & Wraith (2009) for details.
double TChain::get_ln_Z_harmonic(bool use_peak, double nsigma_max, double nsigma_peak, double chain_frac) const {
	// Get the covariance and determinant of the chain
	gsl_matrix* Sigma = gsl_matrix_alloc(N, N);
	gsl_matrix* invSigma = gsl_matrix_alloc(N, N);
	double detSigma;
	stats.get_cov_matrix(Sigma, invSigma, &detSigma);
	
	//std::cout << std::endl << use_peak << "\t" << nsigma_max << "\t" << nsigma_peak << "\t" << chain_frac << std::endl;
	
	/*std::cout << "Covariance:" << std::endl;
	for(unsigned int i=0; i<N; i++) {
		std::cout << std::endl;
		for(unsigned int j=0; j<N; j++) { std::cout << gsl_matrix_get(Sigma, i, j) << "\t"; }
	}
	std::cout << std::endl << std::endl;*/
	
	// Determine the center of the prior volume to use
	double* mu = new double[N];
	if(use_peak) {	// Use the peak density as the center
		find_center(mu, Sigma, invSigma, &detSigma, nsigma_peak, 5);
		//density_peak(mu, nsigma_peak);
	} else {	// Get the mean from the stats class
		for(unsigned int i=0; i<N; i++) { mu[i] = stats.mean(i); }
	}
	//std::cout << std::endl << "mu = ";
	//for(unsigned int i=0; i<N; i++) { std::cout << mu[i] << "\t"; }
	//std::cout << std::endl;
	
	// Sort elements in chain by distance from center, filtering out values of L which are not finite
	std::vector<TChainSort> sorted_indices;
	sorted_indices.reserve(length);
	unsigned int filt_length = 0;
	for(unsigned int i=0; i<length; i++) {
		if(!(isnan(L[i]) || isinf(L[i]))) {
			TChainSort tmp_el;
			tmp_el.index = i;
			tmp_el.dist2 = metric_dist2(invSigma, get_element(i), mu, N);
			sorted_indices.push_back(tmp_el);
			filt_length++;
		}
	}
	unsigned int npoints = (unsigned int)(chain_frac * (double)filt_length);
	std::partial_sort(sorted_indices.begin(), sorted_indices.begin() + npoints, sorted_indices.end());
	/*for(unsigned int i=0; i<20; i++) {
		std::cout << sorted_indices[i].index << "\t" << sorted_indices[i].dist2 << std::endl;
	}*/
	//std::cout << "filtered: " << length - filt_length << std::endl;
	
	// Determine <1/L> inside the prior volume
	double sum_invL = 0.;
	double tmp_invL;
	double nsigma = sqrt(sorted_indices[npoints-1].dist2);
	unsigned int tmp_index = sorted_indices[0].index;;
	double L_0 = L[tmp_index];
	//std::cout << "index_0 = " << sorted_indices[0].index << std::endl;
	for(unsigned int i=0; i<npoints; i++) {
		if(sorted_indices[i].dist2 > nsigma_max * nsigma_max) {
			//std::cout << "Counted only " << i << " elements in chain." << std::endl;
			nsigma = nsigma_max;
			break;
		}
		tmp_index = sorted_indices[i].index;
		//std::cout << "\t" << tmp_index << "\t" << L[tmp_index] << std::endl;
		tmp_invL = w[tmp_index] / exp(L[tmp_index] - L_0);
		if(isnan(tmp_invL)) {
			std::cout << "\t\tL, L_0 = " << L[tmp_index] << ", " << L_0 << std::endl;
		}
		if((tmp_invL + sum_invL > 1.e100) && (i != 0)) {
			nsigma = sqrt(sorted_indices[i-1].dist2);
			break;
		}
		sum_invL += tmp_invL;
	}
	//std::cout << "sum_invL = e^(" << -L_0 << ") * " << sum_invL << " = " << exp(-L_0) * sum_invL << std::endl;
	//std::cout << "nsigma = " << nsigma << std::endl;
	
	// Determine the volume normalization (the prior volume)
	double V = sqrt(detSigma) * 2. * pow(SQRTPI * nsigma, (double)N) / (double)(N) / gsl_sf_gamma((double)(N)/2.);
	//std::cout << "V = " << V << std::endl;
	//std::cout << "total_weight = " << total_weight << std::endl << std::endl;
	
	// Return an estimate of ln(Z)
	double lnZ = log(V) - log(sum_invL) + log(total_weight) + L_0;
	
	if(isnan(lnZ)) {
		std::cout << std::endl;
		std::cout << "NaN Error! lnZ = " << lnZ << std::endl;
		std::cout << "\tsum_invL = e^(" << -L_0 << ") * " << sum_invL << " = " << exp(-L_0) * sum_invL << std::endl;
		std::cout << "\tV = " << V << std::endl;
		std::cout << "\ttotal_weight = " << total_weight << std::endl;
		std::cout << std::endl;
	} else if(isinf(lnZ)) {
		std::cout << std::endl;
		std::cout << "inf Error! lnZ = " << lnZ << std::endl;
		std::cout << "\tsum_invL = e^(" << -L_0 << ") * " << sum_invL << " = " << exp(-L_0) * sum_invL << std::endl;
		std::cout << "\tV = " << V << std::endl;
		std::cout << "\ttotal_weight = " << total_weight << std::endl;
		std::cout << "\tnsigma = " << nsigma << std::endl;
		std::cout << "\tIndex\tDist^2:" << std::endl;
		for(unsigned int i=0; i<10; i++) {
			std::cout << sorted_indices[i].index << "\t\t" << sorted_indices[i].dist2 << std::endl;
			std::cout << "  ";
			const double *tmp_x = get_element(sorted_indices[i].index);
			for(unsigned int k=0; k<N; k++) { std::cout << " " << tmp_x[k]; }
			std::cout << std::endl;
		}
		std::cout << "mu =";
		for(unsigned int i=0; i<N; i++) { std::cout << " " << mu[i]; }
		std::cout << std::endl;
	}
	
	//std::cout << "mu =";
	//for(unsigned int i=0; i<N; i++) { std::cout << " " << mu[i]; }
	//std::cout << std::endl;
	
	// Cleanup
	gsl_matrix_free(Sigma);
	gsl_matrix_free(invSigma);
	delete[] mu;
	
	return lnZ;
	//return V / sum_invL * total_weight;
}


// Estimate the coordinate with peak density.
void TChain::density_peak(double* const peak, double nsigma) const {
	// Width of bin in each direction
	double* width = new double[N];
	uint64_t* index_width = new uint64_t[N];
	uint64_t* mult = new uint64_t[N];
	mult[0] = 1;
	//std::cout << std::endl;
	for(unsigned int i=0; i<N; i++) {
		index_width[i] = (uint64_t)ceil((x_max[i] - x_min[i]) / (nsigma * sqrt(stats.cov(i,i))));
		width[i] = (x_max[i] - x_min[i]) / (double)(index_width[i]);
		//std::cout << x_min[i] << "\t" << x_max[i] << "\t" << width[i] << "\t" << index_width[i] << std::endl;
		if(i != 0) { mult[i] = mult[i-1] * index_width[i-1]; }
	}
	
	// Bin the chain
	std::map<uint64_t, double> bins;
	uint64_t index;
	std::map<uint64_t, double>::iterator it;
	for(unsigned int i=0; i<length; i++) {
		index = 0;
		for(unsigned int k=0; k<N; k++) { index += mult[k] * (uint64_t)floor((x[N*i + k] - x_min[k]) / width[k]); }
		bins[index] += w[i];
	}
	
	// Find the index of the max bin
	std::map<uint64_t, double>::iterator it_end = bins.end();
	double w_max = -std::numeric_limits<double>::infinity();
	for(it = bins.begin(); it != it_end; ++it) {
		if(it->second > w_max) {
			//std::cout << "\t" << it->second << "\t" << it->first << std::endl;
			w_max = it->second;
			index = it->first;
		}
	}
	
	// Convert the index to a coordinate
	//std::cout << index << std::endl;
	uint64_t coord_index;
	for(unsigned int i=0; i<N; i++) {
		coord_index = index % index_width[i];
		index = (index - coord_index) / index_width[i];
		//std::cout << "\t" << coord_index;
		peak[i] = x_min[i] + ((double)coord_index + 0.5) * width[i];
		//std::cout << "\t" << peak[i];
	}
	//std::cout << std::endl;
	//std::cout << index << std::endl;
	
	delete[] width;
	delete[] index_width;
	delete[] mult;
}

// Find a point in space with high density.
void TChain::find_center(double* const center, gsl_matrix *const cov, gsl_matrix *const inv_cov, double* det_cov, double dmax, unsigned int iterations) const {
	// Check that the matrices are the correct size
	/*assert(cov->size1 == N);
	assert(cov->size2 == N);
	assert(inv_cov->size1 == N);
	assert(inv_cov->size2 == N);*/
	
	// Choose random point in chain as starting point
	gsl_rng *r;
	seed_gsl_rng(&r);
	
	long unsigned int index_tmp = gsl_rng_uniform_int(r, length);
	const double *x_tmp = get_element(index_tmp);
	for(unsigned int i=0; i<N; i++) { center[i] = x_tmp[i]; }
	
	//std::cout << "center #0:";
	//for(unsigned int n=0; n<N; n++) { std::cout << " " << center[n]; }
	//std::cout << std::endl;
	
	/*double *E_k = new double[N];
	double *E_ij = new double[N*N];
	for(unsigned int n1=0; n1<N; n1++) {
		E_k[n1] = 0.;
		for(unsigned int n2=0; n2<N; n2++) { E_ij[n1 + N*n2] = 0.; }
	}*/
	
	// Iterate
	double *sum = new double[N];
	double weight;
	for(unsigned int i=0; i<iterations; i++) {
		// Set mean of nearby points as center
		weight = 0.;
		for(unsigned int n=0; n<N; n++) { sum[n] = 0.; }
		for(unsigned int k=0; k<length; k++) {
			x_tmp = get_element(k);
			if(metric_dist2(inv_cov, x_tmp, center, N) < dmax*dmax) {
				for(unsigned int n=0; n<N; n++) { sum[n] += w[k] * x_tmp[n]; }
				weight += w[k];
				
				// Calculate the covariance
				/*if(i == iterations - 1) {
					for(unsigned int n1=0; n1<N; n1++) {
						E_k[n1] += w[k] * x_tmp[n1];
						for(unsigned int n2=0; n2<N; n2++) { E_ij[n1 + N*n2] += w[k] * x_tmp[n1] * x_tmp[n2]; }
					}
				}*/
			}
		}
		//std::cout << "center #" << i+1 << ":";
		for(unsigned int n=0; n<N; n++) { center[n] = sum[n] / (double)weight; }//std::cout << " " << center[n]; }
		//std::cout << " (" << weight << ")" << std::endl;
		
		dmax *= 0.9;
	}
	
	for(unsigned int n=0; n<N; n++) { std::cout << " " << center[n]; }
	std::cout << std::endl;
	
	// Calculate the covariance matrix of the enclosed points
	/*double tmp;
	for(unsigned int i=0; i<N; i++) {
		for(unsigned int j=i; j<N; j++) {
			tmp = (E_ij[i + N*j] - E_k[i]*E_k[j]/(double)weight) / (double)weight;
			gsl_matrix_set(cov, i, j, tmp);
			if(i != j) { gsl_matrix_set(cov, j, i, tmp); }
		}
	}*/
	
	// Get the inverse of the covariance
	/*int s;
	gsl_permutation* p = gsl_permutation_alloc(N);
	gsl_matrix* LU = gsl_matrix_alloc(N, N);
	gsl_matrix_memcpy(LU, cov);
	gsl_linalg_LU_decomp(LU, p, &s);
	gsl_linalg_LU_invert(LU, p, inv_cov);
	
	// Get the determinant of the covariance
	*det_cov = gsl_linalg_LU_det(LU, s);
	
	// Cleanup
	gsl_matrix_free(LU);
	gsl_permutation_free(p);
	delete[] E_k;
	delete[] E_ij;*/
	
	gsl_rng_free(r);
	delete[] sum;
}

void TChain::fit_gaussian_mixture(TGaussianMixture *gm, unsigned int iterations) {
	assert(gm->ndim == N);
	gm->expectation_maximization(x.data(), w.data(), w.size(), iterations);
}


bool TChain::save(std::string fname, std::string group_name,
                   std::string dim_name, int compression, hsize_t chunk, int subsample) const {
	if((compression<0) || (compression > 9)) {
		std::cerr << "! Invalid gzip compression level: " << compression << std::endl;
		return false;
	}
	
	H5::Exception::dontPrint();
	
	H5::H5File *file = H5Utils::openFile(fname);
	if(file == NULL) { return false; }
	
	H5::Group *group = H5Utils::openGroup(file, group_name);
	if(group == NULL) {
		delete file;
		return false;
	}
	
	/*
	 *  Attributes
	 */
	
	// Datatype
	H5::CompType att_type(sizeof(TChainAttribute));
	hid_t tid = H5Tcopy(H5T_C_S1);
	H5Tset_size(tid, H5T_VARIABLE);
	att_type.insertMember("dim_name", HOFFSET(TChainAttribute, dim_name), tid);
	att_type.insertMember("total_weight", HOFFSET(TChainAttribute, total_weight), H5::PredType::NATIVE_FLOAT);
	att_type.insertMember("ndim", HOFFSET(TChainAttribute, ndim), H5::PredType::NATIVE_UINT64);
	att_type.insertMember("length", HOFFSET(TChainAttribute, length), H5::PredType::NATIVE_UINT64);
	
	// Dataspace
	int att_rank = 1;
	hsize_t att_dim = 1;
	H5::DataSpace att_space(att_rank, &att_dim);
	
	// Dataset
	H5::Attribute att = group->createAttribute("attributes", att_type, att_space);
	
	TChainAttribute att_data;
	att_data.dim_name = new char[dim_name.size()+1];
	std::strcpy(att_data.dim_name, dim_name.c_str());
	att_data.total_weight = total_weight;
	att_data.ndim = N;
	att_data.length = length;
	
	att.write(att_type, &att_data);
	
	// Creation property list to be used for all three datasets
	H5::DSetCreatPropList plist;
	plist.setDeflate(compression);	// gzip compression level
	float fillvalue = 0;
	plist.setFillValue(H5::PredType::NATIVE_FLOAT, &fillvalue);
	
	/*
	 *  Choose subsample of points in chain
	 */
	size_t *el_idx = NULL;
	size_t *subsample_idx = NULL;
	if(subsample > 0) {
		size_t tot_weight_tmp = (size_t)ceil(total_weight);
		el_idx = new size_t[tot_weight_tmp];
		size_t unrolled_idx = 0;
		size_t chain_idx = 0;
		std::vector<double>::const_iterator it, it_end;
		it_end = w.end();
		for(it = w.begin(); it != it_end; ++it, chain_idx++) {
			for(size_t n = unrolled_idx; n < unrolled_idx + (size_t)(*it); n++) {
				el_idx[n] = chain_idx;
			}
			unrolled_idx += (size_t)(*it);
		}
		
		assert(chain_idx == length);
		
		gsl_rng *r;
		seed_gsl_rng(&r);
		subsample_idx = new size_t[tot_weight_tmp];
		for(size_t i=0; i<subsample; i++) {
			subsample_idx[i] = el_idx[gsl_rng_uniform_int(r, tot_weight_tmp)];
		}
	}
	
	
	/*
	 *  Coordinates
	 */
	
	// Dataspace
	hsize_t dim;
	if(subsample > 0) {
		dim = subsample;
	} else {
		dim = length;
	}
	// Chunking (required for compression)
	int rank = 2;
	hsize_t coord_dim[2] = {dim, N};
	//if(dim < chunk) {
	plist.setChunk(rank, &(coord_dim[0]));
	//} else {
	//	plist.setChunk(rank, &chunk);
	//}
	H5::DataSpace x_dspace(rank, &(coord_dim[0]));
	
	// Dataset
	//std::stringstream x_dset_path;
	//x_dset_path << group_name << "/chain/coords";
	H5::DataSet* x_dataset = new H5::DataSet(group->createDataSet("coords", H5::PredType::NATIVE_FLOAT, x_dspace, plist));
	
	// Write
	float *buf = new float[N*dim];
	if(subsample > 0) {
		size_t tmp_idx;
		for(size_t i=0; i<subsample; i++) {
			tmp_idx = subsample_idx[i];
			for(size_t k=0; k<N; k++) {
				buf[N*i+k] = x[N*tmp_idx+k];
			}
		}
	} else {
		for(size_t i=0; i<dim; i++) { buf[i] = x[i]; }
	}
	x_dataset->write(buf, H5::PredType::NATIVE_FLOAT);
	
	
	/*
	 *  Weights
	 */
	
	// Dataspace
	if(subsample > 0) {
		dim = subsample;
	} else {
		dim = w.size();
	}
	rank = 1;
	//if(dim < chunk) {
	plist.setChunk(rank, &dim);
	//} else {
	//	plist.setChunk(rank, &chunk);
	//}
	H5::DataSpace w_dspace(rank, &dim);
	
	// Dataset
	//std::stringstream w_dset_path;
	//w_dset_path << group_name << "/chain/weights";
	H5::DataSet* w_dataset = new H5::DataSet(group->createDataSet("weights", H5::PredType::NATIVE_FLOAT, w_dspace, plist));
	
	// Write
	if(subsample > 0) {
		for(size_t i=0; i<subsample; i++) { buf[i] = 1.; }
	} else {
		assert(w.size() < x.size());
		for(size_t i=0; i<w.size(); i++) { buf[i] = w[i]; }
	}
	w_dataset->write(buf, H5::PredType::NATIVE_FLOAT);
	
	
	/*
	 *  Probability densities
	 */
	
	// Dataspace
	//dim = L.size();
	//if(dim < chunk) {
	plist.setChunk(rank, &dim);
	//} else {
	//	plist.setChunk(rank, &chunk);
	//}
	H5::DataSpace L_dspace(rank, &dim);
	
	// Dataset
	//std::stringstream L_dset_path;
	//L_dset_path << group_name << "/chain/probs";
	H5::DataSet* L_dataset = new H5::DataSet(group->createDataSet("ln_p", H5::PredType::NATIVE_FLOAT, L_dspace, plist));
	
	// Write
	if(subsample > 0) {
		for(size_t i=0; i<subsample; i++) { buf[i] = L[subsample_idx[i]]; }
	} else {
		assert(L.size() < x.size());
		for(size_t i=0; i<L.size(); i++) { buf[i] = L[i]; }
	}
	L_dataset->write(buf, H5::PredType::NATIVE_FLOAT);
	
	
	if(subsample > 0) {
		delete[] el_idx;
		delete[] subsample_idx;
	}
	
	delete[] buf;
	delete[] att_data.dim_name;
	
	delete x_dataset;
	delete w_dataset;
	delete L_dataset;
	
	delete group;
	delete file;
	
	return true;
}

bool TChain::load(std::string filename, bool reserve_extra) {
	std::fstream in(filename.c_str(), std::ios::in | std::ios::binary);
	
	if(!in.good()) { return false; }
	
	in.read(reinterpret_cast<char *>(&N), sizeof(unsigned int));
	in.read(reinterpret_cast<char *>(&length), sizeof(unsigned int));
	in.read(reinterpret_cast<char *>(&capacity), sizeof(unsigned int));
	in.read(reinterpret_cast<char *>(&total_weight), sizeof(double));
	
	if(!reserve_extra) {
		capacity = length;
	}
	
	x.reserve(N*capacity);
	L.reserve(capacity);
	w.reserve(capacity);
	
	x.resize(length);
	L.resize(length);
	w.resize(length);
	
	in.read(reinterpret_cast<char *>(&(x[0])), N * length * sizeof(double));
	in.read(reinterpret_cast<char *>(&(L[0])), length * sizeof(double));
	in.read(reinterpret_cast<char *>(&(w[0])), length * sizeof(double));
	
	if(in.fail()) {
		in.close();
		return false;
	}
	
	std::streampos read_offset = in.tellg();
	in.close();
	
	bool stats_success = stats.read_binary(filename, read_offset);
	
	return stats_success;
}

void TChain::get_image(cv::Mat& mat, const TRect& grid, unsigned int dim1, unsigned int dim2,
                       bool norm, double sigma1, double sigma2, double nsigma) const {
	assert((dim1 >= 0) && (dim1 < N) && (dim2 >= 0) && (dim2 < N) && (dim1 != dim2));
	
	mat = cv::Mat::zeros(grid.N_bins[0], grid.N_bins[1], CV_64F);
	
	unsigned int i1, i2;
	for(size_t i=0; i<length; i++) {
		if(grid.get_index(x[N*i+dim1], x[N*i+dim2], i1, i2)) {
			mat.at<double>(i1, i2) += w[i];
			//std::cerr << mat.at<double>(i1, i2) << std::endl;
		}
	}
	
	if(norm) { mat /= total_weight; }
	
	if((sigma1 >= 0.) && (sigma2 >= 0.)) {
		double s1 = sigma1 / grid.dx[0];
		double s2 = sigma2 / grid.dx[1];
		int w1 = 2*ceil(nsigma*s1)+1;
		int w2 = 2*ceil(nsigma*s2)+1;
		
		cv::GaussianBlur(mat, mat, cv::Size(w1,w2), s1, s2, cv::BORDER_REPLICATE);
	}
}


TRect::TRect(double _min[2], double _max[2], unsigned int _N_bins[2]) {
	for(size_t i=0; i<2; i++) {
		min[i] = _min[i];
		max[i] = _max[i];
		N_bins[i] = _N_bins[i];
		dx[i] = (max[i] - min[i]) / (double)N_bins[i];
	}
}

TRect::~TRect() { }

bool TRect::get_index(double x1, double x2, unsigned int& i1, unsigned int& i2) const {
	if((x1 < min[0]) || (x1 >= max[0]) || (x2 < min[1]) || (x2 >= max[1])) {
		return false;
	}
	
	i1 = (x1 - min[0]) / dx[0];
	i2 = (x2 - min[1]) / dx[1];
	
	return true;
}

TRect& TRect::operator=(const TRect& rhs) {
	if(&rhs != this) {
		for(size_t i=0; i<2; i++) {
			min[i] = rhs.min[i];
			max[i] = rhs.max[i];
			N_bins[i] = rhs.N_bins[i];
			dx[i] = rhs.dx[i];
		}
	}
	return *this;
}




/*
 * TGaussianMixture member functions
 * 
 */

TGaussianMixture::TGaussianMixture(unsigned int _ndim, unsigned int _nclusters) 
	: ndim(_ndim), nclusters(_nclusters)
{
	w = new double[nclusters];
	mu = new double[ndim*nclusters];
	cov = new gsl_matrix*[nclusters];
	inv_cov = new gsl_matrix*[nclusters];
	sqrt_cov = new gsl_matrix*[nclusters];
	for(unsigned int k=0; k<nclusters; k++) {
		cov[k] = gsl_matrix_alloc(ndim, ndim);
		inv_cov[k] = gsl_matrix_alloc(ndim, ndim);
		sqrt_cov[k] = gsl_matrix_alloc(ndim, ndim);
	}
	det_cov = new double[nclusters];
	LU = gsl_matrix_alloc(ndim, ndim);
	p = gsl_permutation_alloc(ndim);
	esv = gsl_eigen_symmv_alloc(ndim);
	eival = gsl_vector_alloc(ndim);
	eivec = gsl_matrix_alloc(ndim, ndim);
	sqrt_eival = gsl_matrix_alloc(ndim, ndim);
	seed_gsl_rng(&r);
}

TGaussianMixture::~TGaussianMixture() {
	delete[] w;
	delete[] mu;
	for(unsigned int k=0; k<nclusters; k++) {
		gsl_matrix_free(cov[k]);
		gsl_matrix_free(inv_cov[k]);
		gsl_matrix_free(sqrt_cov[k]);
	}
	delete[] cov;
	delete[] inv_cov;
	delete[] sqrt_cov;
	delete[] det_cov;
	gsl_matrix_free(LU);
	gsl_permutation_free(p);
	gsl_eigen_symmv_free(esv);
	gsl_vector_free(eival);
	gsl_matrix_free(eivec);
	gsl_matrix_free(sqrt_eival);
	gsl_rng_free(r);
}

// Accessors
gsl_matrix* TGaussianMixture::get_cov(unsigned int k) { return cov[k]; }
double TGaussianMixture::get_w(unsigned int k) { return w[k]; }
double* TGaussianMixture::get_mu(unsigned int k) { return &(mu[k*ndim]); }


void TGaussianMixture::invert_covariance() {
	for(unsigned int k=0; k<nclusters; k++) {
		det_cov[k] = invert_matrix(cov[k], inv_cov[k], p, LU);
	}
}

// Calculate the density of the mixture at a series of N points stored in x.
// The result is stored in res.
// 
// Inputs:
//     x[N * ndim]  points at which to evaluate the Gaussian density
//     N            # of points
// 
// Output:
//     res[nclusters * N]    Gaussian density at given points
//
void TGaussianMixture::density(const double *x, unsigned int N, double *res) {
	double sum;
	for(unsigned int n=0; n<N; n++) {
		for(unsigned int k=0; k<nclusters; k++) {
			sum = 0;
			for(unsigned int i=0; i<ndim; i++) {
				for(unsigned int j=0; j<ndim; j++) {
					sum += (x[n*ndim + i] - mu[k*ndim + i]) * gsl_matrix_get(inv_cov[k], i, j) * (x[n*ndim + j] - mu[k*ndim + j]);
				}
			}
			res[nclusters*n + k] = exp(-sum / 2.) / sqrt(2. * 3.14159265358979 * det_cov[k]);
		}
	}
}

double TGaussianMixture::density(const double *x) {
	double sum;
	double res = 0.;
	for(unsigned int k=0; k<nclusters; k++) {
		sum = 0;
		for(unsigned int i=0; i<ndim; i++) {
			for(unsigned int j=0; j<ndim; j++) {
				sum += (x[i] - mu[k*ndim + i]) * gsl_matrix_get(inv_cov[k], i, j) * (x[j] - mu[k*ndim + j]);
			}
		}
		res += exp(-sum / 2.) / sqrt(2. * 3.14159265358979 * det_cov[k]);
	}
	return res;
}

void TGaussianMixture::expectation_maximization(const double *x, const double *w_n, unsigned int N, unsigned int iterations) {
	double *p_kn = new double[nclusters*N];
	
	// Determine total weight
	double sum_w = 0.;
	for(unsigned int n=0; n<N; n++) { sum_w += w_n[n]; }
	
	// Choose means randomly from given points
	unsigned int *index = new unsigned int[nclusters];
	bool repeated_index;
	for(unsigned int k=0; k<nclusters; k++) {
		repeated_index = true;
		while(repeated_index) {
			index[k] = gsl_rng_uniform_int(r, N);
			repeated_index = false;
			for(unsigned int i=0; i<k; i++) {
				if(index[i] == index[k]) { repeated_index = true; break; }
			}
		}
		for(unsigned int i=0; i<ndim; i++) { mu[k*ndim + i] = x[index[k]*ndim + i]; }
	}
	delete[] index;
	
	// Assign points to nearest cluster center
	double sum, tmp, min_dist;
	unsigned int nearest_cluster;
	for(unsigned int n=0; n<N; n++) {
		// Find nearest cluster center
		min_dist = std::numeric_limits<double>::infinity();
		for(unsigned int k=0; k<nclusters; k++) {
			sum = 0.;
			for(unsigned int i=0; i<ndim; i++) {
				tmp = x[n*ndim + i] - mu[k*ndim + i];
				sum += tmp*tmp;
			}
			if(sum < min_dist) {
				min_dist = sum;
				nearest_cluster = k;
			}
		}
		for(unsigned int k=0; k<nclusters; k++) { p_kn[n*nclusters + k] = 0.; }
		p_kn[n*nclusters + nearest_cluster] = 1.;
		w[nearest_cluster] += w_n[n];
	}
	sum = 0.;
	for(unsigned int k=0; k<nclusters; k++) { sum += w[k]; }
	for(unsigned int k=0; k<nclusters; k++) { w[k] /= sum; }
	
	
	// Iterate
	for(unsigned int count=0; count<=iterations; count++) {
		// Assign probability for each point to be in each cluster
		if(count != 0) {
			density(x, N, p_kn);
			for(unsigned int n=0; n<N; n++) {
				// Normalize probability for point to be in some cluster to unity
				sum = 0.;
				for(unsigned int k=0; k<nclusters; k++) { sum += p_kn[n*nclusters + k]; }
				for(unsigned int k=0; k<nclusters; k++) { p_kn[n*nclusters + k] /= sum; }
			}
		}
		
		// Determine cluster properties from members
		if(count != 0) {
			for(unsigned int k=0; k<nclusters; k++) {	// Strength of Gaussian
				w[k] = 0.;
				for(unsigned int n=0; n<N; n++) {
					w[k] += w_n[n] * p_kn[n*nclusters + k];
				}
				w[k] /= sum_w;
			}
		}
		for(unsigned int k=0; k<nclusters; k++) {	// Mean of Gaussian
			for(unsigned int j=0; j<ndim; j++) {
				mu[k*ndim + j] = 0.;
				for(unsigned int n=0; n<N; n++) {
					mu[k*ndim + j] += w_n[n] * p_kn[n*nclusters + k] * x[n*ndim + j] / w[k];
				}
				mu[k*ndim + j] /= sum_w;
			}
		}
		for(unsigned int k=0; k<nclusters; k++) {	// Covariance
			for(unsigned int i=0; i<ndim; i++) {
				for(unsigned int j=i; j<ndim; j++) {
					sum = 0.;
					tmp = 0.;
					for(unsigned int n=0; n<N; n++) {
						sum += p_kn[n*nclusters + k] * w_n[n] * (x[n*ndim + i] - mu[k*ndim + i]) * (x[n*ndim + j] - mu[k*ndim + j]);
						tmp += w_n[n] * p_kn[n*nclusters + k];
					}
					sum /= tmp;
					if(i == j) {
						gsl_matrix_set(cov[k], i, j, 1.01*sum + 0.01);
					} else {
						gsl_matrix_set(cov[k], i, j, sum);
						gsl_matrix_set(cov[k], j, i, sum);
					}
				}
			}
		}
		invert_covariance();
		
		/*std::cout << "Iteration #" << count << std::endl;
		std::cout << "=======================================" << std::endl;
		for(unsigned int k=0; k<nclusters; k++) {
			std::cout << "Cluster #" << k+1 << std::endl;
			std::cout << "w = " << w[k] << std::endl;
			std::cout << "mu =";
			for(unsigned int i=0; i<ndim; i++) {
				std::cout << " " << mu[k*ndim + i];
			}
			std::cout << std::endl;
			std::cout << "Covariance:" << std::endl;
			for(unsigned int i=0; i<ndim; i++) {
				for(unsigned int j=0; j<ndim; j++) {
					std::cout << " " << gsl_matrix_get(cov[k], i, j);
				}
				std::cout << std::endl;
			}
			std::cout << std::endl;
		}*/
		//w_k = np.einsum('n,kn->k', w, p_kn) / N
		//mu = np.einsum('k,n,kn,nj->kj', 1./w_k, w, p_kn, x) / N
		//for j in xrange(k):
		//	Delta[j] = x - mu[j]
		//cov = np.einsum('kn,n,kni,knj->kij', p_kn, w, Delta, Delta)
		//cov = np.einsum('kij,k->kij', cov, 1./np.sum(p_kn, axis=1))
	}
	
	// Find the vector sqrt_cov s.t. sqrt_cov sqrt_cov^T = cov.
	for(unsigned int k=0; k<nclusters; k++) {
		sqrt_matrix(cov[k], sqrt_cov[k], esv, eival, eivec, sqrt_eival);
	}
	
	// Cleanup
	delete[] p_kn;
}


void TGaussianMixture::draw(double* x) {
	double tmp = gsl_rng_uniform(r);
	double sum = 0.;
	for(unsigned int k=0; k<nclusters; k++) {
		sum += w[k];
		if(sum >= tmp) {
			for(unsigned int i=0; i<ndim; i++) { x[i] = mu[k*ndim + i]; }
			for(unsigned int j=0; j<ndim; j++) {
				tmp = gsl_ran_gaussian_ziggurat(r, 1.);
				for(unsigned int i=0; i<ndim; i++) { x[i] += gsl_matrix_get(sqrt_cov[k], i, j) * tmp; }
			}
			break;
		}
	}
}


void TGaussianMixture::print() {
	for(unsigned int k=0; k<nclusters; k++) {
		std::cout << "Cluster #" << k+1 << std::endl;
		std::cout << "w = " << w[k] << std::endl;
		std::cout << "mu =";
		for(unsigned int i=0; i<ndim; i++) {
			std::cout << " " << mu[k*ndim + i];
		}
		std::cout << std::endl;
		std::cout << "Covariance:" << std::endl;
		for(unsigned int i=0; i<ndim; i++) {
			for(unsigned int j=0; j<ndim; j++) {
				std::cout << " " << gsl_matrix_get(cov[k], i, j);
			}
			std::cout << std::endl;
		}
		std::cout << std::endl;
	}
}


/*
 * Image I/O
 */

bool save_mat_image(cv::Mat& img, TRect& rect, std::string fname, std::string group_name,
                    std::string dset_name, std::string dim1, std::string dim2, int compression) {
	assert((img.dims == 2) && (img.rows == rect.N_bins[0]) && (img.cols == rect.N_bins[1]));
	
	if((compression<0) || (compression > 9)) {
		std::cerr << "! Invalid gzip compression level: " << compression << std::endl;
		return false;
	}
	
	H5::Exception::dontPrint();
	
	H5::H5File *file = H5Utils::openFile(fname);
	if(file == NULL) { return false; }
	
	H5::Group *group = H5Utils::openGroup(file, group_name);
	if(group == NULL) {
		delete file;
		return false;
	}
	
	/*
	 *  Image Data
	 */
	
	// Creation property list
	H5::DSetCreatPropList plist;
	int rank = 2;
	hsize_t dim[2] = {rect.N_bins[0], rect.N_bins[1]};
	plist.setDeflate(compression);	// gzip compression level
	float fillvalue = 0;
	plist.setFillValue(H5::PredType::NATIVE_FLOAT, &fillvalue);
	plist.setChunk(rank, &(dim[0]));
	H5::DataSpace dspace(rank, &(dim[0]));
	
	H5::DataSet* dataset;
	try {
		dataset = new H5::DataSet(group->createDataSet(dset_name, H5::PredType::NATIVE_FLOAT, dspace, plist));
	} catch(H5::FileIException create_dset_err) {
		std::cerr << "Unable to create dataset '" << dset_name << "'." << std::endl;
		delete group;
		delete file;
		return false;
	}
	
	float *buf = new float[rect.N_bins[0]*rect.N_bins[1]];
	for(size_t j=0; j<rect.N_bins[0]; j++) {
		for(size_t k=0; k<rect.N_bins[1]; k++) {
			buf[rect.N_bins[1]*j + k] = img.at<double>(j,k);
			/*float tmp = img.at<double>(j,k);
			if(tmp > 0.) {
				std::cerr << j << ", " << k << " --> " << j + rect.N_bins[0]*k << " --> " << tmp << std::endl;
			}*/
		}
	}
	dataset->write(buf, H5::PredType::NATIVE_FLOAT);
	
	/*
	 *  Attributes
	 */
	
	hsize_t att_dim = 2;
	H5::DataSpace att_dspace(1, &att_dim);
	
	H5::PredType att_dtype = H5::PredType::NATIVE_UINT32;
	H5::Attribute att_N = dataset->createAttribute("N_pix", att_dtype, att_dspace);
	att_N.write(att_dtype, &(rect.N_bins));
	
	att_dtype = H5::PredType::NATIVE_DOUBLE;
	H5::Attribute att_min = dataset->createAttribute("min", att_dtype, att_dspace);
	att_min.write(att_dtype, &(rect.min));
	
	att_dtype = H5::PredType::NATIVE_DOUBLE;
	H5::Attribute att_max = dataset->createAttribute("max", att_dtype, att_dspace);
	att_max.write(att_dtype, &(rect.max));
	
	att_dim = 1;
	H5::StrType vls_type(0, H5T_VARIABLE);
	H5::DataSpace att_space_str(H5S_SCALAR);
	H5::Attribute att_name_1 = dataset->createAttribute("dim_name_1", vls_type, att_space_str);
	att_name_1.write(vls_type, dim1);
	H5::Attribute att_name_2 = dataset->createAttribute("dim_name_2",  vls_type, att_space_str);
	att_name_2.write(vls_type, dim2);
	
	file->close();
	
	delete[] buf;
	delete dataset;
	delete group;
	delete file;
	
	return true;
	
}


/*
 * Linear Algebra Functions
 * 
 */

// Sets inv_A to the inverse of A, and returns the determinant of A. If inv_A is NULL, then
// A is inverted in place. If worspaces p and LU are provided, the function does not have to
// allocate its own workspaces.
double invert_matrix(gsl_matrix* A, gsl_matrix* inv_A, gsl_permutation* p, gsl_matrix* LU) {
	unsigned int N = A->size1;
	assert(N == A->size2);
	
	// Allocate workspaces if none are provided
	bool del_p = false;
	bool del_LU = false;
	if(p == NULL) { p = gsl_permutation_alloc(N); del_p = true; }
	if(LU == NULL) { LU = gsl_matrix_alloc(N, N); del_LU = true; }
	
	int s;
	int status = 1;
	unsigned int count = 0;
	while(status) {
		if(count > 5) { std::cerr << "Error inverting matrix." << std::endl; abort(); }
		
		// Invert A using LU decomposition
		gsl_matrix_memcpy(LU, A);
		if(count != 0) { gsl_matrix_add_diagonal(LU, 0.001); std::cerr << "Added small constant" << std::endl; }	// If inversion fails the first time, add small constant to diagonal
		gsl_linalg_LU_decomp(LU, p, &s);
		if(inv_A == NULL) {
			status = gsl_linalg_LU_invert(LU, p, A);
		} else {
			assert(N == inv_A->size1);
			assert(N == inv_A->size2);
			status = gsl_linalg_LU_invert(LU, p, inv_A);
		}
		
		count++;
	}
	
	// Get the determinant of A
	double det_A = gsl_linalg_LU_det(LU, s);
	
	// Free workspaces if none were provided
	if(del_p) { gsl_permutation_free(p); }
	if(del_LU) { gsl_matrix_free(LU); }
	
	return det_A;
}

// Find B s.t. B B^T = A. This is useful for generating vectors from a multivariate normal distribution.
void sqrt_matrix(gsl_matrix* A, gsl_matrix* sqrt_A, gsl_eigen_symmv_workspace* esv, gsl_vector *eival, gsl_matrix *eivec, gsl_matrix* sqrt_eival) {
	size_t N = A->size1;
	assert(A->size2 == N);
	
	// Allocate workspaces if none are provided
	bool del_esv = false;
	if(esv == NULL) { esv = gsl_eigen_symmv_alloc(N); del_esv = true; }
	bool del_eival = false;
	if(eival == NULL) { eival = gsl_vector_alloc(N); del_eival = true; }
	bool del_eivec = false;
	if(eivec == NULL) { eivec = gsl_matrix_alloc(N, N); del_eival = true; }
	bool del_sqrt_eival = false;
	if(sqrt_eival == NULL) {
		sqrt_eival = gsl_matrix_calloc(N, N);
		del_sqrt_eival = true;
	} else {
		assert(sqrt_eival->size1 == N);
		assert(sqrt_eival->size2 == N);
		gsl_matrix_set_zero(sqrt_eival);
	}
	
	if(sqrt_A == NULL) {
		sqrt_A = A;
	} else {
		assert(sqrt_A->size1 == N);
		assert(sqrt_A->size2 == N);
		gsl_matrix_memcpy(sqrt_A, A);
	}
	
	// Calculate the eigendecomposition of the covariance matrix
	gsl_eigen_symmv(sqrt_A, eival, eivec, esv);
	double tmp;
	for(size_t i=0; i<N; i++) {
		tmp = gsl_vector_get(eival, i);
		gsl_matrix_set(sqrt_eival, i, i, sqrt(fabs(tmp)));
		if(tmp < 0.) {
			for(size_t j=0; j<N; j++) { gsl_matrix_set(eivec, j, i, -gsl_matrix_get(eivec, j, i)); }
		}
	}
	gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, 1., eivec, sqrt_eival, 0., sqrt_A);
	
	// Free workspaces if none were provided
	if(del_sqrt_eival) { gsl_matrix_free(sqrt_eival); }
	if(del_esv) { gsl_eigen_symmv_free(esv); }
	if(del_eivec) { gsl_matrix_free(eivec); }
	if(del_eival) { gsl_vector_free(eival); }
}

// Draw a normal varariate from a covariance matrix. The square-root of the covariance (as defined in sqrt_matrix) must be provided.
void draw_from_cov(double* x, const gsl_matrix* sqrt_cov, unsigned int N, gsl_rng* r) {
	double tmp;
	for(unsigned int i=0; i<N; i++) { x[i] = 0.; }
	for(unsigned int j=0; j<N; j++) {
		tmp = gsl_ran_gaussian_ziggurat(r, 1.);
		for(unsigned int i=0; i<N; i++) { x[i] += gsl_matrix_get(sqrt_cov, i, j) * tmp; }
	}
}
