#include "arap.h"

#include <iostream>
#include <map>
#include <tuple>

void ARAP::registration(const std::vector<float>& vertices, const std::vector<unsigned int>& indices)
{
	registration_similarity(vertices, indices);
	//registration_base(vertices, indices);
}

/*
	Min ||(v1' - v0') - (v1 - v0)||
*/
void ARAP::registration_base(const std::vector<float>& vertices, const std::vector<unsigned int> &indices)
{
	int row = indices.size() * 2, col = vertices.size() / 3 * 2;

	A1_error = Eigen::MatrixXf::Zero(row, col);
	b1_error = Eigen::MatrixXf::Zero(row, 1);

	int curRow = 0;
	for (int i = 0; i < indices.size(); i += 3) {
		for (int j = i; j < i + 3; j++) {
			int vStart = indices[j], vEnd = indices[(j + 1 == i + 3 ? i : j + 1)];

			// x part
			A1_error(curRow, vStart * 2) = -1;
			A1_error(curRow, vEnd * 2) = 1;
			b1_error(curRow, 0) = vertices[vEnd * 3] - vertices[vStart * 3];

			// y part
			A1_error(curRow + 1, vStart * 2 + 1) = -1;
			A1_error(curRow + 1, vEnd * 2 + 1) = 1;
			b1_error(curRow + 1, 0) = vertices[vEnd * 3 + 1] - vertices[vStart * 3 + 1];
			
			curRow += 2;
		}
	}

}

/*
	Min ||(v1' - v0') - R * (v1 - v0)||
	R is 2d rotation matrix
*/
void ARAP::registration_similarity(const std::vector<float>& vertices, const std::vector<unsigned int> &indices)
{
	int row = indices.size() * 2, col = vertices.size() / 3 * 2;

	A1_error = Eigen::MatrixXf::Zero(row, col);
	b1_error = Eigen::MatrixXf::Zero(row, 1);

	std::map<std::pair<unsigned int, unsigned int>, std::vector<unsigned int>> edgeTableForTriangleIndex;
	for (int i = 0; i < indices.size(); i += 3) {
		for (int j = i; j < i + 3; j++) {
			int vStart = indices[j], vEnd = indices[(j + 1 == i + 3 ? i : j + 1)];

			edgeTableForTriangleIndex[std::make_pair(vStart, vEnd)].push_back(i / 3);
			edgeTableForTriangleIndex[std::make_pair(vEnd, vStart)].push_back(i / 3);
		}
	}

	int curRow = 0;
	for (int i = 0; i < indices.size(); i += 3) {
		for (int j = i; j < i + 3; j++) {
			int vStart = indices[j], vEnd = indices[(j + 1 == i + 3 ? i : j + 1)];

			// Find other two edges near edge (vj - vi), denoted as ek
			if (edgeTableForTriangleIndex[std::make_pair(vEnd, vStart)].size() > 1) {
				int vIndices[4] = { 0 };  // vi, vj, vl, vr;
				vIndices[0] = vStart;
				vIndices[1] = vEnd;

				for (int k = 0; k < 2; k++) {
					int triangleIndex = edgeTableForTriangleIndex[std::make_pair(vEnd, vStart)][k];

					for (int t = 0; t < 3; t++) {
						int vIndex = indices[triangleIndex * 3 + t];
						if (vIndex != vStart && vIndex != vEnd) {
							vIndices[k + 2] = vIndex;
						}
					}
				}

				float vx[4] = { 0. }, vy[4] = { 0. };

				vx[0] = vertices[vIndices[0] * 3];	vy[0] = vertices[vIndices[0] * 3 + 1];
				vx[1] = vertices[vIndices[1] * 3];	vy[1] = vertices[vIndices[1] * 3 + 1];
				vx[2] = vertices[vIndices[2] * 3];	vy[2] = vertices[vIndices[2] * 3 + 1];
				vx[3] = vertices[vIndices[3] * 3];	vy[3] = vertices[vIndices[3] * 3 + 1];

				/*
					This part of the paper is wrong(maybe).
					I recalculate this part myself, using {ck, sk} = G * {ej, el, er}
				*/

				Eigen::MatrixXf Gk = Eigen::MatrixXf::Zero(6, 2);

				Gk(0, 0) = vx[1] - vx[0];	Gk(0, 1) =	vy[1] - vy[0];
				Gk(1, 0) = vy[1] - vy[0];	Gk(2, 1) =	-(vx[1] - vx[0]);
				Gk(2, 0) = vx[2] - vx[0];	Gk(2, 1) =	vy[2] - vy[0];
				Gk(3, 0) = vy[2] - vy[0];	Gk(3, 1) =	-(vx[2] - vx[0]);
				Gk(4, 0) = vx[3] - vx[0];	Gk(4, 1) =	vy[3] - vy[0];
				Gk(5, 0) = vy[3] - vy[0];	Gk(5, 1) =	-(vx[3] - vx[0]);

				Eigen::MatrixXf G = (Gk.transpose() * Gk).completeOrthogonalDecomposition().pseudoInverse() * Gk.transpose();  // Size of H_ is (2, 6)

				/*
					Here, we come back to the paper.
					{ck, sk} = G * {ej, el, er} ==> {ck, sk} = H * {vi, vj, vl, vr}
				*/

				Eigen::MatrixXf H = Eigen::MatrixXf::Zero(2, 8);

				H(0, 0) = -(G(0, 0) + G(0, 2) + G(0, 4));
				H(0, 1) = -(G(0, 1) + G(0, 3) + G(0, 5));
				H(0, 2) = G(0, 0);
				H(0, 3) = G(0, 1);
				H(0, 4) = G(0, 2);
				H(0, 5) = G(0, 3);
				H(0, 6) = G(0, 4);
				H(0, 7) = G(0, 5);

				H(1, 0) = -(G(1, 0) + G(1, 2) + G(1, 4));
				H(1, 1) = -(G(1, 1) + G(1, 3) + G(1, 5));
				H(1, 2) = G(1, 0);
				H(1, 3) = G(1, 1);
				H(1, 4) = G(1, 2);
				H(1, 5) = G(1, 3);
				H(1, 6) = G(1, 4);
				H(1, 7) = G(1, 5);

				G_all.push_back(H);

				Eigen::MatrixXf t = Eigen::MatrixXf::Zero(2, 2);
				t(0, 0) = vx[1] - vx[0];	t(0, 1) = vy[1] - vy[0];
				t(1, 0) = vy[1] - vy[0];	t(1, 1) = -(vx[1] - vx[0]);

				H = -t * H;

				H(0, 0) += -1;	H(0, 2) += 1;
				H(1, 1) += -1;	H(1, 3) += 1;

				// x part
				A1_error(curRow, vIndices[0] * 2		)	= H(0, 0);
				A1_error(curRow, vIndices[0] * 2 + 1	)	= H(0, 1);
				A1_error(curRow, vIndices[1] * 2		)	= H(0, 2);
				A1_error(curRow, vIndices[1] * 2 + 1	)	= H(0, 3);
				A1_error(curRow, vIndices[2] * 2		)	= H(0, 4);
				A1_error(curRow, vIndices[2] * 2 + 1	)	= H(0, 5);
				A1_error(curRow, vIndices[3] * 2		)	= H(0, 6);
				A1_error(curRow, vIndices[3] * 2 + 1	)	= H(0, 7);

				// y part
				A1_error(curRow + 1, vIndices[0] * 2		) = H(1, 0);
				A1_error(curRow + 1, vIndices[0] * 2 + 1	) = H(1, 1);
				A1_error(curRow + 1, vIndices[1] * 2		) = H(1, 2);
				A1_error(curRow + 1, vIndices[1] * 2 + 1	) = H(1, 3);
				A1_error(curRow + 1, vIndices[2] * 2		) = H(1, 4);
				A1_error(curRow + 1, vIndices[2] * 2 + 1	) = H(1, 5);
				A1_error(curRow + 1, vIndices[3] * 2		) = H(1, 6);
				A1_error(curRow + 1, vIndices[3] * 2 + 1	) = H(1, 7);
			}
			else {  // If only one edge nearby

				int vIndices[3] = { 0 };  // vi, vj, vl;
				vIndices[0] = vStart;
				vIndices[1] = vEnd;

				int triangleIndex = edgeTableForTriangleIndex[std::make_pair(vEnd, vStart)][0];

				for (int t = 0; t < 3; t++) {
					int vIndex = indices[triangleIndex * 3 + t];
					if (vIndex != vStart && vIndex != vEnd) {
						vIndices[2] = vIndex;
					}
				}

				float vx[3] = { 0. }, vy[3] = { 0. };

				vx[0] = vertices[vIndices[0] * 3];	vy[0] = vertices[vIndices[0] * 3 + 1];
				vx[1] = vertices[vIndices[1] * 3];	vy[1] = vertices[vIndices[1] * 3 + 1];
				vx[2] = vertices[vIndices[2] * 3];	vy[2] = vertices[vIndices[2] * 3 + 1];

				Eigen::MatrixXf Gk = Eigen::MatrixXf::Zero(4, 2);

				Gk(0, 0) = vx[1] - vx[0];	Gk(0, 1) = vy[1] - vy[0];
				Gk(1, 0) = vy[1] - vy[0];	Gk(2, 1) = -(vx[1] - vx[0]);
				Gk(2, 0) = vx[2] - vx[0];	Gk(2, 1) = vy[2] - vy[0];
				Gk(3, 0) = vy[2] - vy[0];	Gk(3, 1) = -(vx[2] - vx[0]);

				Eigen::MatrixXf G = (Gk.transpose() * Gk).completeOrthogonalDecomposition().pseudoInverse() * Gk.transpose();  // Size of G is (2, 4)

				Eigen::MatrixXf H = Eigen::MatrixXf::Zero(2, 6);

				// x part
				H(0, 0) = -(G(0, 0) + G(0, 2));
				H(0, 1) = -(G(0, 1) + G(0, 3));
				H(0, 2) = G(0, 0);
				H(0, 3) = G(0, 1);
				H(0, 4) = G(0, 2);
				H(0, 5) = G(0, 3);

				// y part
				H(1, 0) = -(G(1, 0) + G(1, 2));
				H(1, 1) = -(G(1, 1) + G(1, 3));
				H(1, 2) = G(1, 0);
				H(1, 3) = G(1, 1);
				H(1, 4) = G(1, 2);
				H(1, 5) = G(1, 3);

				G_all.push_back(H);

				Eigen::MatrixXf t = Eigen::MatrixXf::Zero(2, 2);
				t(0, 0) = vx[1] - vx[0];	t(0, 1) = vy[1] - vy[0];
				t(1, 0) = vy[1] - vy[0];	t(1, 1) = -(vx[1] - vx[0]);

				H = -t * H;

				H(0, 0) += -1;	H(0, 2) += 1;
				H(1, 1) += -1;	H(1, 3) += 1;

				// x part
				A1_error(curRow, vIndices[0] * 2		) = H(0, 0);
				A1_error(curRow, vIndices[0] * 2 + 1	) = H(0, 1);
				A1_error(curRow, vIndices[1] * 2		) = H(0, 2);
				A1_error(curRow, vIndices[1] * 2 + 1	) = H(0, 3);
				A1_error(curRow, vIndices[2] * 2		) = H(0, 4);
				A1_error(curRow, vIndices[2] * 2 + 1	) = H(0, 5);

				// y part
				A1_error(curRow + 1, vIndices[0] * 2		) = H(1, 0);
				A1_error(curRow + 1, vIndices[0] * 2 + 1	) = H(1, 1);
				A1_error(curRow + 1, vIndices[1] * 2		) = H(1, 2);
				A1_error(curRow + 1, vIndices[1] * 2 + 1	) = H(1, 3);
				A1_error(curRow + 1, vIndices[2] * 2		) = H(1, 4);
				A1_error(curRow + 1, vIndices[2] * 2 + 1	) = H(1, 5);
			}

			curRow += 2;
		}
	}

	//printf("A1_error:\n");
	//std::cout << A1_error << std::endl;
	//std::cout << b1_error << std::endl;

	Eigen::MatrixXf v = Eigen::MatrixXf::Zero(col, 1);

	//printf("v:\n");
	//std::cout << v << std::endl;

	for (int i = 0; i < v.rows(); i += 2) {
		v(i, 0) = vertices[i / 2 * 3];
		v(i + 1, 0) = vertices[i / 2 * 3 + 1];
	}

	printf("A * v\n");
	std::cout << A1_error * v << std::endl;
}

/*
	Min ||v' - v||
	Add constraints for select vetices.
*/
void ARAP::compilation(const std::unordered_set<unsigned int> vSelected)
{
	A1_constraint = Eigen::MatrixXf::Zero(vSelected.size() * 2, A1_error.cols());
	b1_constraint = Eigen::MatrixXf::Zero(vSelected.size() * 2, 1);

	int curRow = 0;
	w = 100.f;  // w increases, constraints increases.
	for (auto iter = vSelected.begin(); iter != vSelected.end(); iter++) {
		int index = *iter;
		A1_constraint(curRow, index * 2) = w;
		A1_constraint(curRow + 1, index * 2 + 1) = w;
		curRow += 2;

		//printf("choose index: %d\n", index);
	}

	//printf("A1_constraint:\n");
	//std::cout << A1_constraint << std::endl;
}

void ARAP::solve(std::vector<float>& vertices, const std::unordered_set<unsigned int> vSelected)
{
	// Update constraint vertices
	int curRow = 0;
	for (auto iter = vSelected.begin(); iter != vSelected.end(); iter++) {
		int index = *iter;
		b1_constraint(curRow, 0) = w * vertices[index * 3];
		b1_constraint(curRow + 1, 0) = w * vertices[index * 3 + 1];
		curRow += 2;
	}

	Eigen::MatrixXf A(A1_error.rows() + A1_constraint.rows(), A1_error.cols());
	A << A1_error, A1_constraint;

	//printf("A:\n");
	//std::cout << A << std::endl;

	Eigen::MatrixXf b(b1_error.rows() + b1_constraint.rows(), b1_error.cols());
	b << b1_error, b1_constraint;
	
	//printf("b:\n");
	//std::cout << b << std::endl;

	// v = (AT * A)^-1 * AT * b
	Eigen::MatrixXf pinv = (A.transpose() * A).completeOrthogonalDecomposition().pseudoInverse();
	Eigen::MatrixXf v = pinv * A.transpose() * b;

	//printf("v:\n");
	//std::cout << v << std::endl;

	for (int i = 0; i < v.rows(); i += 2) {
		vertices[i / 2 * 3] = v(i, 0);
		vertices[i / 2 * 3 + 1] = v(i + 1, 0);
	}

	return;
}
